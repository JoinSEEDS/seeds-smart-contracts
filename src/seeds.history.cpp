#include <seeds.history.hpp>

void history::reset(name account) {
  require_auth(get_self());

  history_tables history(get_self(), account.value);
  auto hitr = history.begin();
  
  while(hitr != history.end()) {
      hitr = history.erase(hitr);
  }
  
  transaction_tables transactions(get_self(), get_self().value);
  auto titr = transactions.begin();
  
  while (titr != transactions.end()) {
    titr = transactions.erase(titr);
  }
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

void history::trxentry(name from, name to, asset quantity, string memo) {
  require_auth(get_self());
  
  auto uitr1 = users.find(from.value);
  auto uitr2 = users.find(to.value);
  
  if (uitr1 == users.end() || uitr2 == users.end()) {
    return;
  }
  
  name fromstatus = uitr1->status;
  name tostatus = uitr2->status;
  
  transaction_tables transactions(get_self(), get_self().value);
  
  transactions.emplace(_self, [&](auto& item) {
    item.id = transactions.available_primary_key();
    item.from = from;
    item.to = to;
    item.quantity = quantity;
    item.memo = memo;
    item.fromstatus = fromstatus;
    item.tostatus = tostatus;
  });
}