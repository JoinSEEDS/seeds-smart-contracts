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

void history::orgtxpoints(name organization) {
  orgtxpt(organization, 0, 200, 0);
}

void history::orgtxpt(name organization, uint64_t id, uint64_t chunksize, uint64_t running_total) {
  auto three_moon_cycles = utils::moon_cycle * 3;
  auto now = eosio::current_time_point().sec_since_epoch();
  auto cutoffdate = now - three_moon_cycles;
  uint64_t count = 0;
  
  org_tx_tables orgtx(get_self(), organization.value);

  if (id == 0) {
    // Step 0 - remove all old transactions
    auto transactions_by_time = orgtx.get_index<"bytimestamp"_n>();
    auto tx_time_itr = transactions_by_time.begin();
    while(tx_time_itr != transactions_by_time.end() && tx_time_itr -> timestamp < cutoffdate && count < chunksize) {
      tx_time_itr = transactions_by_time.erase(tx_time_itr);
      count++;
    }
  }

  if (count < chunksize) {
    // Step 1 - add up values
    
  }


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