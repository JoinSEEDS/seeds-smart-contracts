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

    auto users_by_planted = balances.get_index<"byplanted"_n>();

    uint64_t np = 0;
    auto pitr = users_by_planted.begin();
    while (pitr != users_by_planted.end()) {
      np++;
      pitr++;
    }
    np--;
    
    uint64_t planted_total_reward = config.find(name("hrvstreward").value)->value;
    uint64_t base_planted_reward = (2 * planted_total_reward) / (np * np + np);
    uint64_t total_planted_reward = 0;

    pitr = users_by_planted.begin();
    uint64_t current_user_number = 1;

    while (pitr != users_by_planted.end()) {
      if (pitr->account != get_self()) {
        uint64_t user_planted_reward = base_planted_reward * current_user_number;
      
        init_balance(pitr->account);
  
        auto bitr = balances.find(pitr->account.value);
        balances.modify(bitr, _self, [&](auto& user) {
            user.reward += asset(user_planted_reward, seeds_symbol);
            user.contribution_score = user_planted_score;
        });

        total_planted_reward += user_planted_reward;
      }
     
      pitr++;
      current_user_number++;
    }

    auto titr = balances.find(_self.value);
    balances.modify(titr, _self, [&](auto& total) {
        total.reward += asset(total_planted_reward, seeds_symbol);
    });
}

/*
void harvest::contribution() {
    uint64_t contribution_position = (volume_position + stake_position + rep_position) / 3
    uint64_t users_number = 100
    uint64_t contribution_position = x
    x = contribution_position * 100 / users_number

    float contribution_score = contribution_position * 100 / users_number

    uint64_t total_reward = E(number_users, basic_reward, basic_reward * current_user)
}
*/

/*
void harvest::onperiod() {
  require_auth(_self);

  transaction_tables transactions(name("token"), seeds_symbol.code().raw());

  auto users_by_volume = transactions.get_index<"bytrxvolume"_n>();
  auto users_by_stake = balances.get_index<"byplanted"_n>();
  auto users_by_rep = users.get_index<"byreputation"_n>();

  auto vitr = users_by_volume.begin();
  auto sitr = users_by_stake.begin();
  auto ritr = users_by_rep.begin();

  uint64_t volume_users = std::distance(users_by_volume.end(), users_by_volume.begin());
  uint64_t stake_users = std::distance(users_by_stake.end(), users_by_stake.begin());
  uint64_t rep_users = std::distance(users_by_rep.end(), users_by_rep.begin());

  uint64_t volume_reward = 1000;
  uint64_t stake_reward = 1000;
  uint64_t rep_reward = 1000; 

  uint64_t base_volume_reward = (2 * volume_reward) / (volume_users * volume_users + volume_users);
  uint64_t base_stake_reward = (2 * stake_reward) / (stake_users * stake_users + stake_users);
  uint64_t base_rep_reward = (2 * rep_reward) / (rep_users * rep_users + rep_users);

  uint64_t users_number = config.find("totalusers"_n);
  uint64_t users_with_volume_number = config.find("volumeusers"_n);
  uint64_t total_reward = config.find("totalreward"_n);
  uint64_t contribution_score_reward = total_reward * config.find("cspercent");
  uint64_t total_volume_reward = contribution_score_reward * config.find("volumereward"_n);
  uint64_t total_stake_reward = contribution_score_reward * config.find("stakereward"_n);
  uint64_t total_reputation_reward = contribution_score_reward * config.find("repreward"_n);


  uint64_t base_volume_reward = (2 * total_volume_reward) / (tuwv * tuwv + tuwv);
  uint64_t base_stake_reward = (2 * total_stake_reward) / (tuws * tuws + tuws);
  uint64_t base_reputation_reward = (2 * total_reputation_reward) / (tuws * tuws + tuws);

  {
      uint64_t current_user_number = 1;
      while (vitr != users_by_volume.end()) {
        uint64_t user_volume_reward = base_volume_reward * current_user_number;

        auto hitr = balances.find(vitr->account.value);
        balances.modify(hitr, _self, [&](auto& user) {
            user.reward += asset(user_volume_reward, seeds_symbol);
        });

        vitr++;
        current_user_number++;
      }
  }

  {
    uint64_t current_user_number = 1;
    while (sitr != users_by_stake.end()) {
        uint64_t user_stake_reward = base_stake_reward * current_user_number;

        auto hitr = balances.find(sitr->account.value);
        balances.modify(hitr, _self, [&](auto& user) {
            user.reward += asset(user_stake_reward, seeds_symbol);
        });

        sitr++;
        current_user_number++;
    }
  }

  {
    uint64_t current_user_number = 1;
    while(ritr != users_by_rep.end()) {
        uint64_t user_reputation_reward = base_stake_reward * current_user_number;

        auto hitr = balances.find(ritr->account.value);
        balances.modify(hitr, _self, [&](auto& user) {
            user.reward += asset(user_reputation_reward, seeds_symbol);
        });

        ritr++;
        current_user_number++;
    }
  }

  auto htitr = balances.find(_self.value);
  balances.modify(htitr, _self, [&](auto& total) {
    total.reward += asset(3000, seeds_symbol);
  });
}
*/

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
;