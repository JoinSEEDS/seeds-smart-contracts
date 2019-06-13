#include <seeds.harvest.hpp>

void harvest::reset() {
  require_auth(_self);

  auto uitr = users.begin();
  while (uitr != users.end()) {
    init_balance(uitr->account);
    auto bitr = balances.find((uitr->account).value);
    balances.modify(bitr, _self, [&](auto& user) {
      user.planted = asset(0, seeds_symbol);
      user.reward = asset(0, seeds_symbol);
    });
    uitr++;
  }

  init_balance(_self);
  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
      total.planted = asset(0, seeds_symbol);
      total.reward = asset(0, seeds_symbol);
  });
}

void harvest::plant(name from, name to, asset quantity, string memo) {
  if (to == _self) {
    check_user(from);

    init_balance(from);
    init_balance(_self);

    auto bitr = balances.find(from.value);
    balances.modify(bitr, _self, [&](auto& user) {
      user.planted += quantity;
    });

    auto titr = balances.find(_self.value);
    balances.modify(titr, _self, [&](auto& total) {
      total.planted += quantity;
    });

    deposit(quantity);
  }
}

void harvest::unplant(name from, asset quantity) {
  require_auth(from);
  check_user(from);

  auto bitr = balances.find(from.value);
  balances.modify(bitr, from, [&](auto& user) {
    user.planted -= quantity;
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, from, [&](auto& total) {
    total.planted -= quantity;
  });

  withdraw(from, quantity);
}

void harvest::claimreward(name from) {
  require_auth(from);
  check_user(from);

  asset reward;

  auto bitr = balances.find(from.value);
  balances.modify(bitr, from, [&](auto& user) {
    reward = user.reward;
    user.reward = asset(0, seeds_symbol);
    check(reward > user.reward, "no reward");
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, from, [&](auto& total) {
    total.reward -= reward;
  });

  withdraw(from, reward);
}

void harvest::onperiod() {
  require_auth(_self);

  auto uitr = users.begin();
  auto citr = config.find(name("periodreward").value);
  auto titr = balances.find(_self.value);

  uint64_t period_reward = citr->value;
  asset total_planted = titr->planted;

  uint64_t total_reward = 0;

  while (uitr != users.end()) {
    auto bitr = balances.find((uitr->account).value);
    if (bitr != balances.end()) {
      balances.modify(bitr, _self, [&](auto& user) {
        uint64_t user_reward = period_reward * user.planted / total_planted;
        total_reward += user_reward;
        user.reward += asset(user_reward, seeds_symbol);
      });
    }
    uitr++;
  }

  balances.modify(titr, _self, [&](auto& total) {
    total.reward += asset(total_reward, seeds_symbol);
  });
}

void harvest::init_balance(name account)
{
  auto bitr = balances.find(account.value);
  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto& user) {
      user.account = account;
      user.planted = asset(0, seeds_symbol);
      user.reward = asset(0, seeds_symbol);
    });
  }
}

void harvest::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

void harvest::check_asset(asset quantity)
{
  check(quantity.is_valid(), "invalid asset");
  check(quantity.symbol == seeds_symbol, "invalid asset");
}

void harvest::deposit(asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;

  token::transfer_action action{name(token_account), {_self, "active"_n}};
  action.send(_self, name(bank_account), quantity, "");
}

void harvest::withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;

  token::transfer_action action{name(token_account), {name(bank_account), "active"_n}};
  action.send(name(bank_account), beneficiary, quantity, "");
}
