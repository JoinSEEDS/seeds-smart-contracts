#include <seeds.history.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>

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
  
  // make entry for from no matter what
  transaction_tables transactions(get_self(), from.value);
  transactions.emplace(_self, [&](auto& item) {
    item.id = transactions.available_primary_key();
    item.to = to;
    item.quantity = quantity;
    item.timestamp = eosio::current_time_point().sec_since_epoch();
  });

  // Orgs get entry for "to"
  if (to_user->type == "organisation"_n) {
      org_tx_tables orgtx(get_self(), to.value);
      orgtx.emplace(_self, [&](auto& item) {
        item.id = orgtx.available_primary_key();
        item.from = from;
        item.quantity = quantity;
        item.timestamp = eosio::current_time_point().sec_since_epoch();
      });
  }

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
    tx.delay_sec = 1; // DEBUG no delay
    tx.send(from.value, _self);

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