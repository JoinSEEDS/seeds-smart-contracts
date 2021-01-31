#include <seeds.service.hpp>


ACTION service::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }
}


ACTION service::deposit(name from, name to, asset quantity, string memo) {

  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
      to  ==  get_self() &&                     // to here
      quantity.symbol == seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);

    name target = from;

    if (!memo.empty()) {
      std::size_t found = memo.find(string("sponsor "));
      if (found!=std::string::npos) {
        string acct_name = memo.substr(found + 8,string::npos);
        target = name(acct_name);
        check(is_account(target), "Beneficiary sponsor account does not exist or invalid: " + target.to_string());
     }
    }

    check_user(target);

    init_balance(target);
    init_balance(_self);

    add_balance(target, quantity);
  }

}

ACTION service::createinvite(name sponsor, asset transfer_quantity, asset sow_quantity, checksum256 hash) {

  require_auth(get_self());
  
  utils::check_asset(transfer_quantity);
  utils::check_asset(sow_quantity);
  asset total_quantity = asset(transfer_quantity.amount + sow_quantity.amount, seeds_symbol);

  sub_balance(sponsor, total_quantity);

  _transfer(contracts::onboarding, total_quantity, "");

  // void onboarding::invitefor(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash) {
  action(
    permission_level{get_self(), "active"_n},
    contracts::onboarding, 
    "invitefor"_n,
    std::make_tuple(get_self(), sponsor, transfer_quantity, sow_quantity, hash)
  ).send();

}

ACTION service::pause(name account) {
  require_auth(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "service: user " + account.to_string() + " has no balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.is_paused = true;
  });
}

ACTION service::unpause(name account) {
  require_auth(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "service: user " + account.to_string() + " has no balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.is_paused = false;
  });
}

void service::init_balance (name account) {
  auto bitr = balances.find(account.value);
  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto & item){
      item.account = account;
      item.balance = asset(0, utils::seeds_symbol);
      item.is_paused = false;
    });
  }
}

void service::add_balance (name account, asset quantity) {
  utils::check_asset(quantity);

  check_paused(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "service: no balance object found for " + account.to_string());

  auto bcitr = balances.find((get_self()).value);
  check(bcitr != balances.end(), "service: contract has not balance entry");

  balances.modify(bitr, _self, [&](auto & item){
    item.balance += quantity;
  });

  balances.modify(bcitr, _self, [&](auto & item){
    item.balance += quantity;
  });
}

void service::sub_balance (name account, asset quantity) {
  utils::check_asset(quantity);

  check_paused(account);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "service: no balance object found for " + account.to_string());
  check(bitr -> balance.amount >= quantity.amount, "service: overdrawn balance");

  auto bcitr = balances.find((get_self()).value);
  check(bcitr != balances.end(), "service: contract has not balance entry");
  
  balances.modify(bitr, _self, [&](auto & item){
    item.balance -= quantity;
  });

  balances.modify(bcitr, _self, [&](auto & item){
    item.balance -= quantity;
  });
}

void service::check_user (name account) {
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "service: no user");
}

void service::check_paused (name account) {
  auto bitr = balances.get(account.value, "service: no balance object found");
  check(!bitr.is_paused, "service: account is freezed");
}

void service::_transfer (name beneficiary, asset quantity, string memo) {
  utils::check_asset(quantity);
  token::transfer_action action{contracts::token, {get_self(), "active"_n}};
  action.send(get_self(), beneficiary, quantity, memo);
}
