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

  /*
  name user = name("seedsuser555");
  refund_tables refunds(get_self(), user.value);

  auto ritr = refunds.begin();
  while (ritr != refunds.end()) {
    ritr = refunds.erase(ritr);
  }
  */

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
      else{
        ritr++;
      }
    } else {
      ritr++;
    }
  }
  if (total.amount > 0) {
    withdraw(beneficiary, total);
  }
  action(
      permission_level(contracts::history, "active"_n),
      contracts::history,
      "historyentry"_n,
      std::make_tuple(from, string("trackrefund"), total.amount, string(""))
   ).send();
}

void harvest::cancelrefund(name from, uint64_t request_id) {
  require_auth(from);

  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();

  uint64_t totalReplanted = 0;

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;

      if (refund_time > eosio::current_time_point().sec_since_epoch()) {
        auto bitr = balances.find(from.value);
        balances.modify(bitr, _self, [&](auto& user) {
          user.planted += ritr->amount;
          totalReplanted += ritr->amount.amount;
        });

        auto titr = balances.find(_self.value);
        balances.modify(titr, _self, [&](auto& total) {
          total.planted += ritr->amount;
        });

        ritr = refunds.erase(ritr);
      } else {
        ritr++;
      }
    } else {
      ritr++;
    }
  }

  action(
      permission_level(contracts::history, "active"_n),
      contracts::history,
      "historyentry"_n,
      std::make_tuple(from, string("trackcancel"), totalReplanted, string(""))
   ).send();
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

  uint64_t fraction = quantity.amount / 12;
  uint64_t remainder = quantity.amount % 12;

  for (uint64_t week = 1; week <= 12; week++) {

    uint64_t amt = fraction;

    if (week == 12) {
      amt = amt + remainder;
    }

    refunds.emplace(_self, [&](auto& refund) {
      refund.request_id = lastRequestId + 1;
      refund.refund_id = lastRefundId + week;
      refund.account = from;
      refund.amount = asset( amt, quantity.symbol );
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

void harvest::runharvest() {}

void harvest::calcrep() {
  require_auth(_self);

  auto usersrep = reps.get_index<"byreputation"_n>();

  auto users_number = std::distance(usersrep.begin(), usersrep.end());

  uint64_t current_user = 1;

  auto uitr = usersrep.begin();

  while (uitr != usersrep.end()) {
    uint64_t score = (current_user * 100) / users_number;

    auto hitr = harveststat.find(uitr->account.value);
    if (hitr == harveststat.end()) {
      harveststat.emplace(_self, [&](auto& user) {
        user.account = uitr->account;
        user.reputation_score = score;
      });
    } else {
      harveststat.modify(hitr, _self, [&](auto& user) {
        user.reputation_score = score;
      });
    }

    current_user++;
    uitr++;
  }

  transaction trx{};
  trx.actions.emplace_back(
    permission_level(_self, "active"_n),
    _self,
    "calcrep"_n,
    std::make_tuple()
  );
  trx.delay_sec = 10;
  trx.send(eosio::current_time_point().sec_since_epoch() + 10, _self);
}

void harvest::calctrx() {
  require_auth(_self);

  transaction_tables transactions(contracts::token, seeds_symbol.code().raw());

  auto userstrx = transactions.get_index<"bytrxvolume"_n>();

  auto users_number = std::distance(users.begin(), users.end());

  uint64_t current_user = 1;

  auto uitr = userstrx.begin();

  while (uitr != userstrx.end()) {
    auto aitr = users.find(uitr->account.value);
    if (aitr != users.end()) {
      uint64_t score = (current_user * 100) / users_number;

      auto hitr = harveststat.find(uitr->account.value);
      if (hitr == harveststat.end()) {
        harveststat.emplace(_self, [&](auto& user) {
          user.account = uitr->account;
          user.transactions_score = score;
        });
      } else {
        harveststat.modify(hitr, _self, [&](auto& user) {
          user.transactions_score = score;
        });
      }

      current_user++;
    }

    uitr++;
  }

  transaction trx{};
  trx.actions.emplace_back(
    permission_level(_self, "active"_n),
    _self,
    "calctrx"_n,
    std::make_tuple()
  );
  trx.delay_sec = 10;
  trx.send(eosio::current_time_point().sec_since_epoch() + 20, _self);
}

void harvest::calcplanted() {
  require_auth(_self);

  auto users = balances.get_index<"byplanted"_n>();

  uint64_t users_number = std::distance(users.begin(), users.end()) - 1;

  uint64_t current_user = 1;

  auto uitr = users.begin();

  while (uitr != users.end()) {
    if (uitr->account != get_self()) {
      uint64_t score = (current_user * 100) / users_number;

      auto hitr = harveststat.find(uitr->account.value);
      if (hitr == harveststat.end()) {
        harveststat.emplace(_self, [&](auto& user) {
          user.account = uitr->account;
          user.planted_score = score;
          user.transactions_score = 0;
          user.reputation_score = 0;
          user.contribution_score = 0;
        });
      } else {
        harveststat.modify(hitr, _self, [&](auto& user) {
          user.planted_score = score;
        });
      }

      current_user++;
    }

    uitr++;
  }

  transaction trx{};
  trx.actions.emplace_back(
    permission_level(_self, "active"_n),
    _self,
    "calcplanted"_n,
    std::make_tuple()
  );
  trx.delay_sec = 10;
  trx.send(eosio::current_time_point().sec_since_epoch() + 30, _self);
}

void harvest::claimreward(name from) {
  require_auth(from);
  check_user(from);

  auto bitr = balances.find(from.value);
  check(bitr != balances.end(), "no balance");
  check(bitr->reward > asset(0, seeds_symbol), "no reward available");

  asset reward = asset(bitr->reward.amount, seeds_symbol);

  balances.modify(bitr, _self, [&](auto& user) {
    user.reward = asset(0, seeds_symbol);
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
    total.reward -= reward;
  });

  withdraw(from, reward);

  action(
      permission_level(contracts::history, "active"_n),
      contracts::history,
      "historyentry"_n,
      std::make_tuple(from, string("trackreward"), reward.amount, string(""))
   ).send();

}

void harvest::testreward(name from) {
  require_auth(get_self());
  init_balance(from);

  auto bitr = balances.find(from.value);

  balances.modify(bitr, _self, [&](auto& user) {
    user.reward = asset(100000, seeds_symbol);
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
  if (account == contracts::invites) {
    return;
  }
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

  token::transfer_action action{contracts::token, {_self, "active"_n}};
  action.send(_self, contracts::bank, quantity, "");
}

void harvest::withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;

  token::transfer_action action{contracts::token, {contracts::bank, "active"_n}};
  action.send(contracts::bank, beneficiary, quantity, "");
}
