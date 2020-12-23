#include <seeds.bracelet.hpp>


ACTION bracelet::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }
}


ACTION bracelet::deposit (name from, name to, asset quantity, string memo) {

  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
      to  ==  get_self() &&                     // to here
      quantity.symbol == seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);

    name target = from;

    check_user(target);

    init_balance(target);
    init_balance(_self);

    add_balance(target, quantity);

    _deposit(quantity);
  }

}

ACTION bracelet::freeze (name account) {
  require_auth(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "bracelet: user " + account.to_string() + " has no balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.is_freezed = true;
  });
}

ACTION bracelet::unfreeze (name account) {
  require_auth(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "bracelet: user " + account.to_string() + " has no balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.is_freezed = false;
  });
}

ACTION bracelet::withdraw (name account, asset quantity) {
  require_auth(account);
  string memo = "";
  sub_balance(account, quantity);
  _transfer(account, quantity, memo);
}

ACTION bracelet::transfer (name from, name to, asset quantity, string memo) {
  require_auth(permission_level(from, "bracelet"_n));
  check_freeze(from);
  sub_balance(from, quantity);
  _transfer(to, quantity, memo);
}

void bracelet::init_balance (name account) {
  auto bitr = balances.find(account.value);
  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto & item){
      item.account = account;
      item.balance = asset(0, utils::seeds_symbol);
      item.is_freezed = false;
    });
  }
}

void bracelet::add_balance (name account, asset quantity) {
  utils::check_asset(quantity);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "bracelet: no balance object found for " + account.to_string());

  auto bcitr = balances.find((get_self()).value);
  check(bcitr != balances.end(), "bracelet: contract has not balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.balance += quantity;
  });

  balances.modify(bcitr, _self, [&](auto & item){
    item.balance += quantity;
  });
}

void bracelet::sub_balance (name account, asset quantity) {
  utils::check_asset(quantity);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "bracelet: no balance object found for " + account.to_string());
  check(bitr -> balance.amount >= quantity.amount, "bracelet: overdrawn balance");

  auto bcitr = balances.find((get_self()).value);
  check(bcitr != balances.end(), "bracelet: contract has not balance entry");
  
  balances.modify(bitr, _self, [&](auto & item){
    item.balance -= quantity;
  });

  balances.modify(bcitr, _self, [&](auto & item){
    item.balance -= quantity;
  });
}

void bracelet::_deposit (asset quantity) {
  check_asset(quantity);

  token::transfer_action action{contracts::token, {_self, "active"_n}};
  action.send(_self, contracts::bank, quantity, "");
}

void bracelet::check_user (name account) {
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "bracelet: no user");
}

void bracelet::_transfer (name beneficiary, asset quantity, string memo) {
  utils::check_asset(quantity);
  token::transfer_action action{contracts::token, {contracts::bank, "active"_n}};
  action.send(contracts::bank, beneficiary, quantity, memo);
}

void bracelet::check_freeze (name account) {
  auto bitr = balances.get(account.value, "bracelet: no balance object found");
  check(!bitr.is_freezed, "bracelet: account is freezed");
}
