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

  transaction_points_tables transactions(get_self(), account.value);
  auto titr = transactions.begin();
  while (titr != transactions.end()) {
    titr = transactions.erase(titr);
  }

  qev_tables qevs(get_self(), account.value);
  auto qitr = qevs.begin();
  while (qitr != qevs.end()) {
    qitr = qevs.erase(qitr);
  }

  auto citr = citizens.begin();
  while (citr != citizens.end()) {
    citr = citizens.erase(citr);
  }
  
  auto ritr = residents.begin();
  while (ritr != residents.end()) {
    ritr = residents.erase(ritr);
  }

  auto toitr = totals.begin();
  while (toitr != totals.end()) {
    toitr = totals.erase(toitr);
  }

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }

  for (name s : status_names) {
    account_status_tables accstatus(get_self(), s.value);
    auto aitr = accstatus.begin();
    while (aitr != accstatus.end()) {
      aitr = accstatus.erase(aitr);
    }
  }

  auto tcitr = trxcbprewards.begin();
  while (tcitr != trxcbprewards.end()) {
    tcitr = trxcbprewards.erase(tcitr);
  }
}

void history::deldailytrx (uint64_t day) {
  daily_transactions_tables transactions(get_self(), day);
  auto titr = transactions.begin();
  while (titr != transactions.end()) {
    titr = transactions.erase(titr);
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
  size_change("reidents.sz"_n, 1);
}

void history::addcitizen(name account) {
  require_auth(get_self());
  
  check_user(account);
  
  citizens.emplace(get_self(), [&](auto& user) {
    user.id = citizens.available_primary_key();
    user.account = account;
    user.timestamp = eosio::current_time_point().sec_since_epoch();
  });
  size_change("citizens.sz"_n, 1);
}

void history::updatestatus (name account, name scope) {
  require_auth(get_self());

  account_status_tables acctstatus(get_self(), scope.value);
  
  acctstatus.emplace(_self, [&](auto & item){
    item.id = acctstatus.available_primary_key();
    item.account = account;
    item.timestamp = eosio::current_time_point().sec_since_epoch();
  });

  size_change(scope, 1);
}

double history::get_transaction_multiplier (name account, name other) {
  double multiplier = utils::get_rep_multiplier(account);
  
  auto oitr = organizations.find(account.value);

  if (oitr != organizations.end()) {
    multiplier *= config_float_get(name("org" + std::to_string(oitr->status+1) + "trx.mul"));
  }

  auto bitr_account = members.find(account.value);
  auto bitr_other = members.find(other.value);

  if (
    bitr_account != members.end() && 
    bitr_other != members.end() && 
    bitr_account -> region == bitr_other -> region
  ) {
    multiplier *= config_float_get("local.mul"_n);
  }

  return multiplier;
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
  
  if (quantity.symbol != utils::seeds_symbol) {
    return;
  }

  auto from_planted = planted.find(from.value);
  auto to_planted = planted.find(to.value);

  if (from_planted == planted.end() || to_planted == planted.end()) {
    return;
  }

  bool from_is_organization = organizations.find(from.value) != organizations.end();
  bool to_is_organization = organizations.find(to.value) != organizations.end();

  auto from_totals_itr = totals.find(from.value);

  if (from_totals_itr != totals.end()) {
    totals.modify(from_totals_itr, _self, [&](auto & item){
      item.total_volume += quantity.amount;
      item.total_number_of_transactions += 1;
      item.total_outgoing_to_rep_orgs += ( to_is_organization ? 1 : 0 );
    });
  } else {
    totals.emplace(_self, [&](auto & item){
      item.account = from;
      item.total_volume += quantity.amount;
      item.total_number_of_transactions = 1;
      item.total_incoming_from_rep_orgs = 0;
      item.total_outgoing_to_rep_orgs = ( to_is_organization ? 1 : 0 );
    });
  }

  if (from_is_organization) {
    auto to_totals_itr = totals.find(to.value);
    if (to_totals_itr != totals.end()) {
      totals.modify(to_totals_itr, _self, [&](auto & item){
        item.total_incoming_from_rep_orgs += 1;
      });
    } else {
      totals.emplace(_self, [&](auto & item){
        item.account = to;
        item.total_volume = 0;
        item.total_number_of_transactions = 0;
        item.total_incoming_from_rep_orgs = 1;
        item.total_outgoing_to_rep_orgs = 0;
      });
    }
  }

  uint64_t trx_id = get_deferred_id();

  action a(
    permission_level(get_self(), "active"_n),
    get_self(),
    "savepoints"_n,
    std::make_tuple(from, to, quantity, trx_id)
  );

  transaction tx;
  tx.actions.emplace_back(a);
  tx.delay_sec = 1;
  tx.send(trx_id, _self);
}


void history::savepoints (name from, name to, asset quantity, uint64_t trx_id) {
  require_auth(get_self());

  uint64_t day = utils::get_beginning_of_day_in_seconds();
  daily_transactions_tables transactions(get_self(), day);

  bool from_is_organization = organizations.find(from.value) != organizations.end();
  bool to_is_organization = organizations.find(to.value) != organizations.end();

  int64_t transactions_cap = int64_t(config_get("qev.trx.cap"_n));
  int64_t max_transaction_points_individuals = int64_t(config_get("i.trx.max"_n));
  int64_t max_transaction_points_organizations = int64_t(config_get("org.trx.max"_n));

  double from_capped_amount = (
    from_is_organization ? 
    std::min(max_transaction_points_organizations, quantity.amount) : 
    std::min(max_transaction_points_individuals, quantity.amount)
  ) / 10000.0;
  
  double to_capped_amount = std::min(max_transaction_points_organizations, quantity.amount) / 10000.0;
  uint64_t qvolume = std::min(transactions_cap, quantity.amount);

  auto transactions_by_from_to = transactions.get_index<"byfromto"_n>();

  uint64_t max_number_transactions = config_get("htry.trx.max"_n);

  uint128_t from_to_id = (uint128_t(from.value) << 64) + to.value;
  uint64_t count = 0;

  auto ft_itr = transactions_by_from_to.find(from_to_id);
  auto current_itr = ft_itr;

  while (ft_itr != transactions_by_from_to.end() && 
      count < max_number_transactions && 
      ft_itr -> from == from && ft_itr -> to == to) {

    if (ft_itr -> volume < current_itr->volume) {
      current_itr = ft_itr;
    }

    ft_itr++;
    count++;
  }

  bool current_has_smaller_volume = current_itr->volume < qvolume;

  if (count >= max_number_transactions && !current_has_smaller_volume) { return; }

  uint64_t fpoints = uint64_t(ceil(from_capped_amount * get_transaction_multiplier(to, from)));
  uint64_t tpoints = to_is_organization ? uint64_t(ceil(to_capped_amount * get_transaction_multiplier(from, to))) : 0;

  transactions.emplace(_self, [&](auto & transaction){
    transaction.id = transactions.available_primary_key();
    transaction.from = from;
    transaction.to = to;
    transaction.volume = quantity.amount;
    transaction.qualifying_volume = qvolume;
    transaction.from_points = fpoints;
    transaction.to_points = tpoints;
    transaction.timestamp = eosio::current_time_point().sec_since_epoch();
  });

  int64_t from_points = int64_t(fpoints);
  int64_t to_points = int64_t(tpoints);
  int64_t qualifying_volume = int64_t(qvolume);

  if (count >= max_number_transactions && current_has_smaller_volume) {
    from_points -= current_itr->from_points;
    to_points -= current_itr->to_points;
    qualifying_volume -= current_itr->qualifying_volume;

    transactions_by_from_to.erase(current_itr);
  }

  save_from_metrics(from, from_points, qualifying_volume, day);

  if (to_is_organization) {
    transaction_points_tables trx_points_to(get_self(), to.value);
    auto trx_itr_to = trx_points_to.find(day);

    if (trx_itr_to != trx_points_to.end()) {
      trx_points_to.modify(trx_itr_to, _self, [&](auto & item){
        item.points += to_points;
      });
    } else {
      trx_points_to.emplace(_self, [&](auto & item){
        item.timestamp = day;
        item.points = to_points;
      });
    }
  }

  if (from_is_organization) {
    send_update_txpoints(from);
  }

  send_trx_cbp_reward_action(from, to);
}

void history::save_from_metrics (name from, int64_t & from_points, int64_t & qualifying_volume, uint64_t & day) {
  transaction_points_tables trx_points_from(get_self(), from.value);
  qev_tables qevs(get_self(), from.value);
  qev_tables qevs_total(get_self(), get_self().value);

  auto trx_itr = trx_points_from.find(day);
  auto qev_itr = qevs.find(day);
  auto qev_total_itr = qevs_total.find(day);

  if (trx_itr != trx_points_from.end()) {
    trx_points_from.modify(trx_itr, _self, [&](auto & item){
      item.points += from_points;
    });
  } else {
    trx_points_from.emplace(_self, [&](auto & item){
      item.timestamp = day;
      item.points = from_points;
    });
  }

  if (qev_itr != qevs.end()) {
    qevs.modify(qev_itr, _self, [&](auto & item){
      item.qualifying_volume += qualifying_volume;
    });
  } else {
    qevs.emplace(_self, [&](auto & item){
      item.timestamp = day;
      item.qualifying_volume = qualifying_volume;
    });
  }

  if (qev_total_itr != qevs_total.end()) {
    qevs_total.modify(qev_total_itr, _self, [&](auto & item){
      item.qualifying_volume += qualifying_volume;
    });
  } else {
    qevs_total.emplace(_self, [&](auto & item){
      item.timestamp = day;
      item.qualifying_volume = qualifying_volume;
    });
  }
}


void history::send_trx_cbp_reward_action (name from, name to) {
  action a(
    permission_level(get_self(), "active"_n),
    get_self(),
    "sendtrxcbp"_n,
    std::make_tuple(from, to)
  );

  transaction tx;
  tx.actions.emplace_back(a);
  tx.delay_sec = 1;
  tx.send(get_deferred_id(), _self);
}

void history::send_add_cbs (name account, int points) {
  action(
    permission_level(contracts::accounts, "addcbs"_n),
    contracts::accounts,
    "addcbs"_n,
    std::make_tuple(account, points)
  ).send();
}

void history::trx_cbp_reward (name account, name key) {

  auto trxcbprewards_by_acct_key = trxcbprewards.get_index<"byacctkey"_n>();

  uint128_t id = (uint128_t(account.value) << 64) + key.value;
  auto itr = trxcbprewards_by_acct_key.find(id);

  uint64_t now = eosio::current_time_point().sec_since_epoch();

  if (itr != trxcbprewards_by_acct_key.end()) {

    uint64_t threshold = now - utils::moon_cycle;
    if (itr->timestamp > threshold) { return; }

    trxcbprewards_by_acct_key.modify(itr, _self, [&](auto & item){
      item.timestamp = now;
    });

  } else {
    trxcbprewards.emplace(_self, [&](auto & item){
      item.id = trxcbprewards.available_primary_key();
      item.account = account;
      item.key = key;
      item.timestamp = now;
    });
  }

  send_add_cbs(account, int(config_get(key)));
}

void history::sendtrxcbp (name from, name to) {
  require_auth(get_self());

  auto oitr = organizations.find(to.value);

  if (oitr != organizations.end()) {
    if (oitr->status == status_regenerative) {
      trx_cbp_reward(from, "buyregen.cbp"_n);
    }
    if (oitr->status == status_thrivable) {
      trx_cbp_reward(from, "buythriv.cbp"_n);
    }
  }

  auto bitr_from = members.find(from.value);
  auto bitr_to = members.find(to.value);

  if (
    bitr_from != members.end() && 
    bitr_to != members.end() && 
    bitr_from->region == bitr_to->region
  ) {
    trx_cbp_reward(from, "buylocal.cbp"_n);
  }
}

// CAUTION: this will iterate on all citizens, residents and orgs
void history::migrate() {
  require_auth(get_self());
  
  uint32_t count = 0;
  auto ctr = citizens.begin();
  while(ctr != citizens.end()) {
    ctr++;
    count++;
  }
  size_set("citizens.sz"_n, count);

  count = 0;
  auto rtr = residents.begin();
  while(rtr != residents.end()) {
    rtr++;
    count++;
  }
  size_set("residents.sz"_n, count);
}

void history::size_change(name id, int delta) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = delta;
    });
  } else {
    uint64_t newsize = sitr->size + delta; 
    if (delta < 0) {
      if (sitr->size < -delta) {
        newsize = 0;
      }
    }
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

void history::size_set(name id, uint64_t newsize) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = newsize;
    });
  } else {
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

uint64_t history::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}

void history::send_update_txpoints (name from) {
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
  tx.send(get_deferred_id(), _self);
}

void history::numtrx(name account) {
  auto titr = totals.find(account.value);
  uint64_t num = 0;

  if (titr != totals.end()) {
    num = titr -> total_number_of_transactions;
  }

  check(false, "{ numtrx: " + std::to_string(num) + " }");
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

double history::config_float_get(name key) {
  DEFINE_CONFIG_FLOAT_TABLE
  DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX
  config_float_tables config(contracts::settings, contracts::settings.value);

  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}


void history::testtotalqev (uint64_t numdays, uint64_t volume) {
  require_auth(get_self());

  qev_tables qevs_total(get_self(), get_self().value);

  uint64_t day = utils::get_beginning_of_day_in_seconds();
  uint64_t cutoff = day - (numdays * utils::seconds_per_day);
  uint64_t current_day = day;

  while (current_day >= cutoff) {
    auto qitr = qevs_total.find(current_day);

    if (qitr != qevs_total.end()) {
      qevs_total.modify(qitr, _self, [&](auto & item){
        item.qualifying_volume = volume;
      });
    } else {
      qevs_total.emplace(_self, [&](auto & item){
        item.timestamp = current_day;
        item.qualifying_volume = volume;
      });
    }

    current_day -= utils::seconds_per_day;
  }

}


void history::migrateusers () {
  require_auth(get_self());
  uint64_t batch_size = config_get("batchsize"_n);
  migrateuser(0, 0, batch_size);
}

void history::migrateuser (uint64_t start, uint64_t transaction_id, uint64_t chunksize) {
  require_auth(get_self());

  auto uitr = start == 0 ? users.begin() : users.find(start);
  uint64_t count = 0;

  print("batch: start = ", start, ", transaction_id = ", transaction_id, ", chunksize = ", chunksize, "\n");

  while (uitr != users.end() && count < chunksize) {

    if (uitr -> type != "organisation"_n) {
      transaction_tables transactions(get_self(), uitr -> account.value);
      auto titr = transaction_id == 0 ? transactions.begin() : transactions.find(transaction_id);

      while (titr != transactions.end() && count < chunksize) {

        save_migration_user_transaction(uitr -> account, titr -> to, titr -> quantity, titr -> timestamp);
        titr++;
        count += 5;

        if (titr == transactions.end()) {
          transaction_id = 0;
        } else {
          transaction_id = titr -> id;
        }

      }

    } else {
      org_tx_tables transactions(get_self(), uitr -> account.value);
      auto titr = transaction_id == 0 ? transactions.begin() : transactions.find(transaction_id);

      while (titr != transactions.end() && count < chunksize) {
        if (!(titr -> in)) {
          transaction_id = titr -> id;
          save_migration_user_transaction(uitr -> account, titr -> other, titr -> quantity, titr -> timestamp);
        }
        titr++;
        count += 5;

        if (titr == transactions.end()) {
          transaction_id = 0;
        } else {
          transaction_id = titr -> id;
        }

      }

    }

    if (transaction_id == 0) {
      uitr++;
    }
  }

  if (uitr != users.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "migrateuser"_n,
      std::make_tuple(uitr -> account.value, transaction_id, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(get_self().value, _self);
  } else {
    print("\n############################ I have FINISHED ############################\n");
  }

}

void history::save_migration_user_transaction (name from, name to, asset quantity, uint64_t timestamp) {

  auto from_user = users.find(from.value);
  auto to_user = users.find(to.value);

  auto date = eosio::time_point_sec(timestamp / 86400 * 86400);
  uint64_t day = date.utc_seconds;
  daily_transactions_tables transactions(get_self(), day);

  print("saving: from = ", from, ", to = ", to, ", quantity = ", quantity, ", timestamp = ", timestamp, "\n");
  
  uint64_t transaction_id = transactions.available_primary_key();

  bool from_is_organization = from_user -> type == "organisation"_n;
  bool to_is_organization = to_user -> type == "organisation"_n;

  int64_t transactions_cap = int64_t(config_get("qev.trx.cap"_n));
  int64_t max_transaction_points_individuals = int64_t(config_get("i.trx.max"_n));
  int64_t max_transaction_points_organizations = int64_t(config_get("org.trx.max"_n));

  double from_trx_multiplier = (
    from_is_organization ? 
    std::min(max_transaction_points_organizations, quantity.amount) : 
    std::min(max_transaction_points_individuals, quantity.amount)
  ) / 10000.0;

  double to_trx_multiplier = std::min(max_transaction_points_organizations, quantity.amount) / 10000.0;

  transactions.emplace(_self, [&](auto & transaction){
    transaction.id = transaction_id;
    transaction.from = from;
    transaction.to = to;
    transaction.volume = quantity.amount;
    transaction.qualifying_volume = std::min(transactions_cap, quantity.amount);
    transaction.from_points = uint64_t(ceil(from_trx_multiplier * utils::get_rep_multiplier(to)));
    transaction.to_points = uint64_t(ceil(to_trx_multiplier * utils::get_rep_multiplier(from)));
    transaction.timestamp = timestamp;
  });

  auto from_totals_itr = totals.find(from.value);

  if (from_totals_itr != totals.end()) {
    totals.modify(from_totals_itr, _self, [&](auto & item){
      item.total_volume += quantity.amount;
      item.total_number_of_transactions += 1;
      item.total_outgoing_to_rep_orgs += ( to_is_organization ? 1 : 0 );
    });
  } else {
    totals.emplace(_self, [&](auto & item){
      item.account = from;
      item.total_volume += quantity.amount;
      item.total_number_of_transactions = 1;
      item.total_incoming_from_rep_orgs = 0;
      item.total_outgoing_to_rep_orgs = ( to_is_organization ? 1 : 0 );
    });
  }

  if (from_is_organization) {
    auto to_totals_itr = totals.find(to.value);
    if (to_totals_itr != totals.end()) {
      totals.modify(to_totals_itr, _self, [&](auto & item){
        item.total_incoming_from_rep_orgs += 1;
      });
    } else {
      totals.emplace(_self, [&](auto & item){
        item.account = to;
        item.total_volume = 0;
        item.total_number_of_transactions = 0;
        item.total_incoming_from_rep_orgs = 1;
        item.total_outgoing_to_rep_orgs = 0;
      });
    }
  }

  adjust_transactions(transaction_id, timestamp);
}

void history::adjust_transactions (uint64_t id, uint64_t timestamp) {

  auto date = eosio::time_point_sec(timestamp / 86400 * 86400);
  uint64_t day = date.utc_seconds;

  daily_transactions_tables transactions(get_self(), day);
  auto transactions_by_from_to = transactions.get_index<"byfromto"_n>();
  auto titr = transactions.find(id);

  check(titr != transactions.end(), "transaction not found");
  
  name from = titr -> from;
  name to = titr -> to;
  auto uitr_from = users.find(from.value);
  auto uitr_to = users.find(to.value);
  uint64_t max_number_transactions = config_get("htry.trx.max"_n);
  
  uint128_t from_to_id = (uint128_t(titr -> from.value) << 64) + titr -> to.value;
  
  uint64_t count = 0;
  auto ft_itr = transactions_by_from_to.find(from_to_id);
  auto current_itr = ft_itr;
  
  while (ft_itr != transactions_by_from_to.end() && 
      count <= max_number_transactions && 
      ft_itr -> from == from && ft_itr -> to == to) {
    if (ft_itr -> volume < current_itr -> volume) {
      current_itr = ft_itr;
    }
    ft_itr++;
    count++;
  }
  
  int64_t from_points = int64_t(titr -> from_points);
  int64_t to_points = int64_t(titr -> to_points);
  int64_t qualifying_volume = int64_t(titr -> qualifying_volume);
  
  if (count > max_number_transactions) {
    from_points -= current_itr -> from_points;
    to_points -= current_itr -> to_points;
    qualifying_volume -= current_itr -> qualifying_volume;
    transactions_by_from_to.erase(current_itr);
  }
  
  save_from_metrics(from, from_points, qualifying_volume, day);
  
  if (uitr_to -> type == name("organisation")) {
    transaction_points_tables trx_points_to(get_self(), to.value);
    auto trx_itr_to = trx_points_to.find(day);
    if (trx_itr_to != trx_points_to.end()) {
      trx_points_to.modify(trx_itr_to, _self, [&](auto & item){
        item.points += to_points;
      });
    } else {
      trx_points_to.emplace(_self, [&](auto & item){
        item.timestamp = day;
        item.points = to_points;
      });
    }
  }
}

uint64_t history::get_deferred_id () {
  deferred_trx_id_tables deferred_t(get_self(), get_self().value);
  deferred_trx_id_table d_t = deferred_t.get_or_create(get_self(), deferred_trx_id_table());

  d_t.id += 1;

  deferred_t.set(d_t, get_self());

  return d_t.id;
}

