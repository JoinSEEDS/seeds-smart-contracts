#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
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

  name user = name("seedsuser555");
  refund_tables refunds(get_self(), user.value);

  auto ritr = refunds.begin();
  while (ritr != refunds.end()) {
    ritr = refunds.erase(ritr);
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
    require_auth(from);
    check_user(from);
    check_user(to);

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


void harvest::claimrefund(name from, uint64_t request_id) {
  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();
  check(ritr != refunds.end(), "No refund found");

  asset total = asset(0, seeds_symbol);
  name beneficiary = ritr->account;

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;
      if (refund_time < eosio::current_time_point().sec_since_epoch()) {
        total += ritr->amount;
        ritr = refunds.erase(ritr);
      }
    }
  }

  withdraw(beneficiary, total);
}

void harvest::cancelrefund(name from, uint64_t request_id) {
  require_auth(from);

  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;

      if (refund_time > eosio::current_time_point().sec_since_epoch()) {
        auto bitr = balances.find(from.value);
        balances.modify(bitr, _self, [&](auto& user) {
          user.planted += ritr->amount;
        });

        auto titr = balances.find(_self.value);
        balances.modify(titr, _self, [&](auto& total) {
          total.planted += ritr->amount;
        });

        ritr = refunds.erase(ritr);
      }
    }
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
  asset fraction = asset( quantity.amount / 12, quantity.symbol );
  for (uint64_t week = 1; week <= 12; week++) {
    refunds.emplace(_self, [&](auto& refund) {
      refund.request_id = lastRequestId + 1;
      refund.refund_id = lastRefundId + week;
      refund.account = from;
      refund.amount = fraction;
      refund.weeks_delay = week;
      refund.request_time = eosio::current_time_point().sec_since_epoch();
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
    check(user.reward >= asset(0, seeds_symbol), "no reward");
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
    total.reward -= reward;
  });

  withdraw(from, reward);
}

void harvest::upbyplanted() {
  require_auth(_self);

  auto users = balances.get_index<"byplanted"_n>();

  uint64_t total_participants = std::distance(users.begin(), users.end());

  uint64_t current_participant = 1;

  auto uitr = users.begin();

  while (uitr != users.end()) {
    if (uitr->account != get_self()) {
      uint64_t user_score = current_participant * 100 / total_participants;

      auto hsitr = harveststat.find(uitr->account.value);
      harveststat.modify(hsitr, _self, [&](auto& stat) {
        stat.planted_score = current_participant;
      });

      current_participant++;
    }
  }
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
      uint64_t user_planted_score = current_user_number * 100 / np;
      uint64_t user_transactions_score = user_planted_score;
      uint64_t user_reputation_score = user_planted_score;
      uint64_t user_contribution_score = user_planted_score;

      init_balance(pitr->account);

      auto bitr = balances.find(pitr->account.value);
      balances.modify(bitr, _self, [&](auto& user) {
          user.reward += asset(user_planted_reward, seeds_symbol);
      });

      auto hitr = harveststat.find(pitr->account.value);
      if (hitr == harveststat.end()) {
        harveststat.emplace(_self, [&](auto& user) {
            user.account = pitr->account;
            user.reputation_score = user_reputation_score;
            user.planted_score = user_planted_score;
            user.transactions_score = user_transactions_score;
            user.contribution_score = user_contribution_score;
        });
      } else {
        harveststat.modify(hitr, _self, [&](auto& user) {
            user.reputation_score = user_reputation_score;
            user.planted_score = user_planted_score;
            user.transactions_score = user_transactions_score;
            user.contribution_score = user_contribution_score;
        });
      }

      total_planted_reward += user_planted_reward;
    }

    pitr++;
    current_user_number++;
  }

  transaction trx{};
  trx.actions.emplace_back(
    permission_level(_self, "active"_n),
    _self,
    "onperiod"_n,
    std::make_tuple()
  );
  trx.delay_sec = 10;
  trx.send(eosio::current_time_point().sec_since_epoch(), _self);
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
