#include <seeds.history.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <utils.hpp>

void history::reset(name account) {
  require_auth(get_self());

  history_tables history(get_self(), account.value);
  auto hitr = history.begin();
  
  while(hitr != history.end()) {
      hitr = history.erase(hitr);
  }
  
  transaction_tables transactions(get_self(), account.value);
  auto titr = transactions.begin();  
  while (titr != transactions.end()) {
    titr = transactions.erase(titr);
  }
  
  org_tx_tables orgtx(get_self(), account.value);
  auto oitr = orgtx.begin();
  while (oitr != orgtx.end()) {
    oitr = orgtx.erase(oitr);
  }
  
  auto citr = citizens.begin();
  while (citr != citizens.end()) {
    citr = citizens.erase(citr);
  }
  
  auto ritr = residents.begin();
  while (ritr != residents.end()) {
    ritr = residents.erase(ritr);
  }
}

void history::addresident(name account) {
  require_auth(get_self());
  
  check_user(account);
  
  residents.emplace(get_self(), [&](auto& user) {
    user.id = residents.available_primary_key();
    user.account = account;
    user.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void history::addcitizen(name account) {
  require_auth(get_self());
  
  check_user(account);
  
  citizens.emplace(get_self(), [&](auto& user) {
    user.id = citizens.available_primary_key();
    user.account = account;
    user.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void history::addreputable(name organization) {
  require_auth(get_self());

  auto uitr = users.find(organization.value);
  check(uitr != users.end(), "no user found");
  check(uitr -> type == name("organisation"), "the user type must be organization");

  reputables.emplace(_self, [&](auto & org){
    org.id = reputables.available_primary_key();
    org.organization = organization;
    org.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void history::addregen(name organization) {
  require_auth(get_self());

  auto uitr = users.find(organization.value);
  check(uitr != users.end(), "no user found");
  check(uitr -> type == name("organisation"), "the user type must be organization");

  regens.emplace(_self, [&](auto & org){
    org.id = regens.available_primary_key();
    org.organization = organization;
    org.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void history::historyentry(name account, string action, uint64_t amount, string meta) {
  require_auth(get_self());

  history_tables history(get_self(), account.value);

  history.emplace(_self, [&](auto& item) {
    item.history_id = history.available_primary_key();
    item.account = account;
    item.action = action;
    item.amount = amount;
    item.meta = meta;
    item.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void history::trxentry(name from, name to, asset quantity) {
  require_auth(get_self());
  
  auto from_user = users.find(from.value);
  auto to_user = users.find(to.value);
  
  if (from_user == users.end() || to_user == users.end()) {
    return;
  }

  bool from_is_organization = from_user->type == "organisation"_n;
  
  if (from_is_organization) {
    org_tx_tables orgtx(get_self(), from.value);
    orgtx.emplace(_self, [&](auto& item) {
      item.id = orgtx.available_primary_key();
      item.other = to;
      item.in = false;
      item.quantity = quantity;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  } else {
    transaction_tables transactions(get_self(), from.value);
    transactions.emplace(_self, [&](auto& item) {
      item.id = transactions.available_primary_key();
      item.to = to;
      item.quantity = quantity;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  }

  // Orgs get entry for "to"
  if (to_user->type == "organisation"_n) {
      org_tx_tables orgtx(get_self(), to.value);
      orgtx.emplace(_self, [&](auto& item) {
        item.id = orgtx.available_primary_key();
        item.other = from;
        item.in = true;
        item.quantity = quantity;
        item.timestamp = eosio::current_time_point().sec_since_epoch();
      });
  }

  if (!from_is_organization) {
    // delayed update
    cancel_deferred(from.value);

    // delayed update
    cancel_deferred(from.value);

    action a(
        permission_level{contracts::harvest, "active"_n},
        contracts::harvest,
        "updatetxpt"_n,
        std::make_tuple(from)
    );

    transaction tx;
    tx.actions.emplace_back(a);
    tx.delay_sec = 1; 
    tx.send(from.value, _self);
  }

}

// return true if anything was cleaned up, false otherwise
bool history::clean_old_tx(name org, uint64_t chunksize) {
  
  auto now = eosio::current_time_point().sec_since_epoch();
  auto cutoffdate = now - utils::moon_cycle * 3;

  org_tx_tables orgtx(get_self(), org.value);
  uint64_t count = 0;
  auto transactions_by_time = orgtx.get_index<"bytimestamp"_n>();
  auto tx_time_itr = transactions_by_time.begin();
  bool result = false;
  while(tx_time_itr != transactions_by_time.end() && tx_time_itr->timestamp < cutoffdate && count < chunksize) {
    tx_time_itr = transactions_by_time.erase(tx_time_itr);
    result = true;
    count++;
  }  
  return result;
  
}

void history::orgtxpoints(name org) {
  require_auth(get_self());
  orgtxpt(org, 0, 200, 0);
}

void history::orgtxpt(name org, uint128_t start_val, uint64_t chunksize, uint64_t running_total) {
  require_auth(get_self());

  org_tx_tables orgtx(get_self(), org.value);
  uint64_t count = 0;

  if (start_val == 0 && clean_old_tx(org, chunksize)) {    // Step 0 - remove all old transactions
    fire_orgtx_calc(org, 0, chunksize, running_total);
    return;
  }

  // Step 1 - add up values
  uint64_t max_quantity = config_get("org.trx.max"_n);
  uint64_t max_number_of_transactions = config_get("org.trx.div"_n);;

  auto transactions_by_other = orgtx.get_index<"byother"_n>();
  auto tx_other_itr = start_val == 0 ? transactions_by_other.begin() : transactions_by_other.lower_bound(start_val);

  name      current_other = name("");
  uint64_t  current_num = 0;
  double    current_rep_multiplier = 0.0;

  //print("orgtxpt for "+org.to_string());

  while(tx_other_itr != transactions_by_other.end() && count < chunksize) {

    if (current_other != tx_other_itr->other) {
      current_other = tx_other_itr->other;
      current_num = 0;
      current_rep_multiplier = utils::get_rep_multiplier(current_other);
      //print("new other "+current_other.to_string());
      //print("rep mul "+std::to_string(current_rep_multiplier));
    } else {
      //print("same iter other "+current_other.to_string());
      current_num++;
    }

    if (current_num < max_number_of_transactions) {
      uint64_t volume = tx_other_itr->quantity.amount;
      
      //print("; vol: "+std::to_string(volume));

      // limit max volume
      if (volume > max_quantity) {
        volume = max_quantity;
        //print("; vol exceed max - limited to: "+std::to_string(volume));
      }

      // multiply by receiver reputation
      double points = (double(volume) / 10000.0) * current_rep_multiplier;
      //print("; rep mul "+std::to_string(current_rep_multiplier));
      //print("; points "+std::to_string(points));

      running_total += points;
      
      //print("; total "+std::to_string(running_total));
      tx_other_itr++;
    } else {
      tx_other_itr++;
      // TODO skip ahead to next account!
      // rather than increasing account by 1, increase name by 1 and look for a new 
      // iterator starting with that name as lower limit
      // name next_name = name(tx_other_itr->other.value + 1);
      // uunt128_t next_val = uunt128_t(next_name.value) << 64;
      // tx_other_itr = transactions_by_other.lower_bound(next_val);
      // print("; skip to "+std::to_string(next_val));

    }
  
    count++;
  }
  if (tx_other_itr == transactions_by_other.end()) {
    action(
      permission_level{contracts::harvest, "active"_n},
      contracts::harvest, "setorgtxpt"_n,
      std::make_tuple(org, running_total)
    ).send();
  } else {
    uint128_t next_value = tx_other_itr->by_other();
    fire_orgtx_calc(org, next_value, chunksize, running_total);
  }

  

}

void history::fire_orgtx_calc(name org, uint128_t next_value, uint64_t chunksize, uint64_t running_total) {
  action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "orgtxpt"_n,
      std::make_tuple(org, next_value, chunksize, running_total)
  );
  transaction tx;
  tx.actions.emplace_back(next_execution);
  tx.delay_sec = 1;
  tx.send(next_value + 1, _self);
}

void history::numtrx(name account) {
  uint32_t num = num_transactions(account, 200);
  check(false, "{ numtrx: " + std::to_string(num) + " }");
}

// return number of transactions outgoing, until a limit
uint32_t history::num_transactions(name account, uint32_t limit) {
  transaction_tables transactions(contracts::history, account.value);
  auto titr = transactions.begin();
  uint32_t count = 0;
  while(titr != transactions.end() && count < limit) {
    titr++;
    count++;
  }
  return count;
}

void history::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

uint64_t history::config_get(name key) {
  DEFINE_CONFIG_TABLE
  DEFINE_CONFIG_TABLE_MULTI_INDEX
  config_tables config(contracts::settings, contracts::settings.value);

  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}


void history::resetmigrate (name account) {
  require_auth(get_self());
  
  transaction_table_migrations transactions(get_self(), account.value);
  auto titr = transactions.begin();  
  while (titr != transactions.end()) {
    titr = transactions.erase(titr);
  }
  
  org_tx_table_migrations orgtx(get_self(), account.value);
  auto oitr = orgtx.begin();
  while (oitr != orgtx.end()) {
    oitr = orgtx.erase(oitr);
  } 
}

void history::migratebacks () {
  require_auth(get_self());
  migrateback(0, 400);
}

void history::migrateback (uint64_t start, uint64_t chunksize) {
  require_auth(get_self());

  auto uitr = start == 0 ? users.begin() : users.find(start);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    if (uitr -> type != name("organisation")) {
      transaction_tables transactions(get_self(), uitr -> account.value);
      transaction_table_migrations transactions_migration(get_self(), uitr -> account.value);

      auto titr = transactions_migration.begin();
      while (titr != transactions_migration.end()) {
        auto tmigration = transactions.find(titr -> id);
        if (tmigration != transactions.end()) {
          transactions.modify(tmigration, _self, [&](auto & item){
            item.to = titr -> to;
            item.quantity = titr -> quantity;
            item.timestamp = titr -> timestamp;            
          });
        } else {
          transactions.emplace(_self, [&](auto & item){
            item.id = titr -> id;
            item.to = titr -> to;
            item.quantity = titr -> quantity;
            item.timestamp = titr -> timestamp;
          });
        }
        titr++; // titr = transactions_migration.erase(titr); if uncomment, remember to remove titr++
        count++;
      }
      count++;
    } else {
      org_tx_tables transactions(get_self(), uitr -> account.value);
      org_tx_table_migrations transactions_migration(get_self(), uitr -> account.value);

      auto titr = transactions_migration.begin();
      while (titr != transactions_migration.end()) {
        auto tmigration = transactions.find(titr -> id);
        if (tmigration != transactions.end()) {
          transactions.modify(tmigration, _self, [&](auto & item){
            item.other = titr -> other;
            item.in = titr -> in;
            item.quantity = titr -> quantity;
            item.timestamp = titr -> timestamp;            
          });
        } else {
          transactions.emplace(_self, [&](auto & item){
            item.id = titr -> id;
            item.other = titr -> other;
            item.in = titr -> in;
            item.quantity = titr -> quantity;
            item.timestamp = titr -> timestamp;
          });
        }
        titr++; // titr = transactions_migration.erase(titr); if uncomment, remember to remove titr++
        count++;
      }
      count++;
    }
    uitr++;
  }

  if (uitr != users.end()) {
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "migratetrx"_n,
        std::make_tuple(uitr -> account, chunksize)
    );
    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(uitr -> account.value + 1, _self);
  }
}

void history::testtrx (name from, name to, asset quantity) {
  require_auth(get_self());
  
  auto from_user = users.find(from.value);
  auto to_user = users.find(to.value);
  
  if (from_user == users.end() || to_user == users.end()) {
    return;
  }

  bool from_is_organization = from_user->type == "organisation"_n;
  
  if (from_is_organization) {
    org_tx_table_migrations orgtx(get_self(), from.value);
    orgtx.emplace(_self, [&](auto& item) {
      item.id = orgtx.available_primary_key();
      item.other = to;
      item.in = false;
      item.quantity = quantity;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  } else {
    transaction_table_migrations transactions(get_self(), from.value);
    transactions.emplace(_self, [&](auto& item) {
      item.id = transactions.available_primary_key();
      item.to = to;
      item.quantity = quantity;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  }

  // Orgs get entry for "to"
  if (to_user->type == "organisation"_n) {
      org_tx_table_migrations orgtx(get_self(), to.value);
      orgtx.emplace(_self, [&](auto& item) {
        item.id = orgtx.available_primary_key();
        item.other = from;
        item.in = true;
        item.quantity = quantity;
        item.timestamp = eosio::current_time_point().sec_since_epoch();
      });
  }

}