#include <seeds.harvest.hpp>

void harvest::reset() {
  require_auth(_self);

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }

  auto hitr = harveststat.begin();
  while (hitr != harveststat.end()) {
    hitr = harveststat.erase(hitr);
  }

  init_balance(_self);
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

void harvest::sow(name from, name to, asset quantity) {
    // we allow to sow seeds for a friend before he has account
    // check_user(from);

    init_balance(from);
    init_balance(to);

    auto fromitr = balances.find(from.value);
    balances.modify(fromitr, _self, [&](auto& user) {
        user.planted -= quantity;
    });

    auto toitr = balances.find(to.value);
    balances.modify(toitr, _self, [&](auto& user) {
        user.planted += quantity;
    });
}

void harvest::claimrefund(name from) {
  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();

  while (ritr != refunds.end()) {
    uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;

    if (refund_time < current_time()) {
      refunds.erase(ritr);
      withdraw(ritr->account, ritr->amount);
    }

    ritr++;
  }
}

void harvest::cancelrefund(name from) {
  require_auth(from);

  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();

  while (ritr != refunds.end()) {
    uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;

    if (refund_time > current_time()) {
      refunds.erase(ritr);

      auto bitr = balances.find(from.value);
      balances.modify(bitr, _self, [&](auto& user) {
        user.planted += ritr->amount;
      });

      auto titr = balances.find(_self.value);
      balances.modify(titr, _self, [&](auto& total) {
        total.planted += ritr->amount;
      });
    }

    ritr++;
  }
}

void harvest::unplant(name from, asset quantity) {
  require_auth(from);
  check_user(from);

  uint64_t lastRequestId = 0;
  uint64_t lastRefundId = 0;

  refund_tables refunds(get_self(), from.value);
  if (refunds.begin() != refunds.end()) {
    auto ritr = refunds.end();
    ritr--;
    lastRequestId = ritr->request_id;
    lastRefundId = ritr->refund_id;
  }

  for (uint64_t week = 1; week <= 12; week++) {
    refunds.emplace(_self, [&](auto& refund) {
      refund.request_id = lastRequestId + 1;
      refund.refund_id = lastRefundId + week;
      refund.account = from;
      refund.amount = quantity;
      refund.weeks_delay = week;
      refund.request_time = current_time();
    });
  }

  auto bitr = balances.find(from.value);
  balances.modify(bitr, _self, [&](auto& user) {
    user.planted -= quantity;
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
    total.planted -= quantity;
  });
}

void harvest::claimreward(name from, asset reward) {
  require_auth(from);
  check_user(from);
  check_asset(reward);

  init_balance(from);

  auto bitr = balances.find(from.value);
  balances.modify(bitr, _self, [&](auto& user) {
    user.reward = user.reward - reward;
    check(user.reward > asset(0, seeds_symbol), "no reward");
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
    total.reward -= reward;
  });

  withdraw(from, reward);
}


void harvest::onperiod() {
  require_auth(_self);

  transaction_tables transactions(name("seedstoken12"), seeds_symbol.code().raw());

  auto users_by_volume = transactions.get_index<"bytrxvolume"_n>();
  auto users_by_stake = balances.get_index<"byplanted"_n>();
  auto users_by_rep = users.get_index<"byreputation"_n>();

  auto vitr = users_by_volume.begin();
  auto sitr = users_by_stake.begin();
  auto ritr = users_by_rep.begin();

  uint64_t volume_users = std::distance(users_by_volume.begin(), users_by_volume.end());
  uint64_t stake_users = std::distance(users_by_stake.begin(), users_by_stake.end());
  uint64_t rep_users = std::distance(users_by_rep.begin(), users_by_rep.end());

  uint64_t volume_reward = 100000;
  uint64_t stake_reward = 100000;
  uint64_t rep_reward = 100000;
  uint64_t total_reward = 0;

  uint64_t base_volume_reward = (2 * volume_reward) / (volume_users * volume_users + volume_users);
  uint64_t base_stake_reward = (2 * stake_reward) / (stake_users * stake_users + stake_users);
  uint64_t base_rep_reward = (2 * rep_reward) / (rep_users * rep_users + rep_users);

  {
    auto uitr = users.begin();
    while (uitr != users.end()) {
      name account = uitr->account;

      auto hsitr = harveststat.find(account.value);
      if (hsitr == harveststat.end()) {
        harveststat.emplace(_self, [&](auto& stat) {
          stat.account = account;
          stat.contribution_score = 0;
          stat.reputation_score = 0;
          stat.transactions_score = 0;
          stat.planted_score = 0;
        });
      } else {
        harveststat.modify(hsitr, _self, [&](auto& stat) {
          stat.account = account;
          stat.contribution_score = 0;
          stat.reputation_score = 0;
          stat.transactions_score = 0;
          stat.planted_score = 0;
        });
      }

      init_balance(account);

      uitr++;
    }
  }

  {
      uint64_t current_user_number = 1;
      while (vitr != users_by_volume.end()) {
        name account = vitr->account;
        uint64_t user_volume_reward = base_volume_reward * current_user_number;

        auto hitr = balances.find(account.value);
        if (hitr != balances.end()) {
          balances.modify(hitr, _self, [&](auto& user) {
              user.reward += asset(user_volume_reward, seeds_symbol);
          });
        }

        auto hsitr = harveststat.find(account.value);
        if (hsitr != harveststat.end()) {
          harveststat.modify(hsitr, _self, [&](auto& stat) {
            stat.transactions_score = current_user_number;
          });
        }

        total_reward += user_volume_reward;

        vitr++;
        current_user_number++;
      }
  }

  {
    uint64_t current_user_number = 1;
    while (sitr != users_by_stake.end()) {
        name account = sitr->account;
        uint64_t user_stake_reward = base_stake_reward * current_user_number;

        auto hitr = balances.find(account.value);
        if (hitr != balances.end()) {
          balances.modify(hitr, _self, [&](auto& user) {
              user.reward += asset(user_stake_reward, seeds_symbol);
          });
        }

        auto hsitr = harveststat.find(account.value);
        if (hsitr != harveststat.end()) {
          harveststat.modify(hsitr, _self, [&](auto& stat) {
            stat.planted_score = current_user_number;
          });
        }

        total_reward += user_stake_reward;

        sitr++;
        current_user_number++;
    }
  }

  {
    uint64_t current_user_number = 1;
    while(ritr != users_by_rep.end()) {
        name account = ritr->account;
        uint64_t user_reputation_reward = base_rep_reward * current_user_number;

        auto hitr = balances.find(account.value);
        if (hitr != balances.end()) {
          balances.modify(hitr, _self, [&](auto& user) {
              user.reward += asset(user_reputation_reward, seeds_symbol);
          });
        }

        auto hsitr = harveststat.find(account.value);
        if (hsitr != harveststat.end()) {
          harveststat.modify(hsitr, _self, [&](auto& stat) {
            stat.reputation_score = current_user_number;
          });
        }

        total_reward += user_reputation_reward;

        ritr++;
        current_user_number++;
    }
  }

  auto htitr = balances.find(_self.value);
  balances.modify(htitr, _self, [&](auto& total) {
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
