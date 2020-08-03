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

  
  name user = name("seedsuserbbb");
  refund_tables refunds(get_self(), user.value);

  auto ritr = refunds.begin();
  while (ritr != refunds.end()) {
    ritr = refunds.erase(ritr);
  }
  
  auto titr = txpoints.begin();
  while (titr != txpoints.end()) {
    titr = txpoints.erase(titr);
  }

  init_balance(_self);
}

void harvest::plant(name from, name to, asset quantity, string memo) {
  if (to == _self) {
    utils::check_asset(quantity);

    name target = from;

    if (!memo.empty()) {
      std::size_t found = memo.find(string("sow "));
      if (found!=std::string::npos) {
        string target_acct_name = memo.substr (4,string::npos);
        target = name(target_acct_name);
     } else {
        check(false, "invalid memo");
      }
    }

    check_user(target);

    init_balance(target);
    init_balance(_self);

    auto bitr = balances.find(target.value);
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
    check(fromitr->planted.amount >= quantity.amount, "can't sow more than planted!");

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

  auto bitr = balances.find(from.value);
  check(bitr->planted.amount >= quantity.amount, "can't unplant more than planted!");

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

  balances.modify(bitr, _self, [&](auto& user) {
    user.planted -= quantity;
  });

  auto titr = balances.find(_self.value);
  balances.modify(titr, _self, [&](auto& total) {
    total.planted -= quantity;
  });

}

void harvest::runharvest() {}

ACTION harvest::updatetxpt(name account) {
  require_auth(_self);

  calc_tx_points(account);
}


// DEBUG actions to clear scores
ACTION harvest::clearscores() {
  uint64_t limit = 200;

  auto titr = txpoints.begin();
  while (titr != txpoints.end() && limit > 0) {
    titr = txpoints.erase(titr);
    limit--;
  }  

  // auto hitr = harveststat.begin();
  // while (hitr != harveststat.end() && limit > 0) {
  //   hitr = harveststat.erase(hitr);
  //   limit--;
  // }  

  auto citr = cspoints.begin();
  while (citr != cspoints.end() && limit > 0) {
    citr = cspoints.erase(citr);
    limit--;
  }  


}
// Calculate Transaction Points for a single account
void harvest::calc_tx_points(name account) {

  auto three_moon_cycles = moon_cycle * 3;
  auto now = eosio::current_time_point().sec_since_epoch();
  auto cutoffdate = now - three_moon_cycles;

  // get all transactions for this account
  transaction_tables transactions(contracts::history, account.value);

  auto transactions_by_to = transactions.get_index<"byto"_n>();
  auto tx_to_itr = transactions_by_to.rbegin();

  double result = 0;

  uint64_t max_quantity = 1777; // get this from settings // TODO verify this number
  uint64_t max_number_of_transactions = 26;

  name      current_to = name("");
  uint64_t  current_num = 0;
  double    current_rep_multiplier = 0.0;

  while(tx_to_itr != transactions_by_to.rend()) {

    if (tx_to_itr->timestamp < cutoffdate) {

      // remove old transactions
      //tx_to_itr = transactions_by_to.erase(tx_to_itr);
      
      auto it = transactions_by_to.erase(--tx_to_itr.base());
      tx_to_itr = std::reverse_iterator(it);            
    } else {

      // update "to"
      if (current_to != tx_to_itr->to) {
        current_to = tx_to_itr->to;
        current_num = 0;
        current_rep_multiplier = get_rep_multiplier(current_to);
      } else {
        current_num++;
      }

      print("iterating over "+std::to_string(tx_to_itr->id));

      if (current_num < max_number_of_transactions) {
        uint64_t volume = tx_to_itr->quantity.amount;

        // limit max volume
        if (volume > max_quantity * 10000) {
          volume = max_quantity * 10000;
        }

        // multiply by receiver reputation
        double points = (double(volume) / 10000.0) * current_rep_multiplier;

        result += points;

      } 

      tx_to_itr++;
    }
  }

  // use ceil function so each schore is counted if it is > 0
    
  // DEBUG
  if (result == 0) {
    result = 33.0;
  }
  // enter into transaction points table
  auto hitr = txpoints.find(account.value);
  if (hitr == txpoints.end()) {
    txpoints.emplace(_self, [&](auto& entry) {
      entry.account = account;
      entry.points = (uint64_t) ceil(result);
    });
  } else {
    txpoints.modify(hitr, _self, [&](auto& entry) {
      entry.points = (uint64_t) ceil(result);
    });
  }

}

void harvest::calctrxpt() {
    calctrxpts(0, 0, 100);
}

void harvest::calctrxpts(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = utils::get_users_size();
  uint64_t current = chunk * chunksize;
  auto uitr = start_val == 0 ? users.begin() : users.lower_bound(start_val);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    calc_tx_points(uitr->account);

    current++;
    count++;
    uitr++;
  }

}

void harvest::calctrx() {
  require_auth(_self);

  auto users = txpoints.get_index<"bypoints"_n>();

  uint64_t users_number = std::distance(users.begin(), users.end());

  uint64_t current_user = 0;
  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  auto uitr = users.begin();

  while (uitr != users.end()) {
    uint64_t score = (current_user * 100) / users_number;

    if (uitr->points == 0) {
      score = 0;
    }

    auto hitr = harveststat.find(uitr->account.value);

    if (hitr == harveststat.end()) {
        init_harvest_stat(uitr->account);
        hitr = harveststat.find(uitr->account.value);
    } 

    harveststat.modify(hitr, _self, [&](auto& user) {
      user.transactions_score = score;
      user.tx_timestamp = now_time;
    });

    current_user++;

    uitr++;
  }

}

// [PS+RT+CB X Rep = Total Contribution Score]
void harvest::calccs() {

  require_auth(_self);

  check(false, "doesn't work - fix");

  auto hitr = harveststat.begin();
  while (hitr != harveststat.end()) {

    double rep = get_rep_multiplier(hitr->account);

    uint64_t cs_score = uint64_t((hitr->planted_score + hitr->transactions_score + hitr->community_building_score) * rep);

    auto csitr = cspoints.find(hitr->account.value);
    if (csitr == cspoints.end()) {
      cspoints.emplace(_self, [&](auto& item) {
        item.account = hitr->account;
        item.contribution_points = cs_score;
        item.cycle = 0;
      });
    } else {
      cspoints.modify(csitr, _self, [&](auto& item) {
        item.contribution_points = cs_score;
      });
    }

    hitr++;
  }

  auto users = cspoints.get_index<"bycspoints"_n>();

  uint64_t users_number = std::distance(users.begin(), users.end());

  uint64_t current_user = 0;
  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  auto uitr = users.begin();

  while (uitr != users.end()) {
    uint64_t score = (current_user * 100) / users_number;

    if (uitr->contribution_points == 0) {
      score = 0;
    }

    auto hitr = harveststat.find(uitr->account.value);

    if (hitr == harveststat.end()) {
        init_harvest_stat(uitr->account);
        hitr = harveststat.find(uitr->account.value);
    } 

    harveststat.modify(hitr, _self, [&](auto& user) {
      user.contribution_score = score;
      user.contrib_timestamp = now_time;
    });

    current_user++;

    uitr++;
  }

}


void harvest::payforcpu(name account) {
    require_auth(get_self()); // satisfied by payforcpu permission
    require_auth(account);

    auto uitr = users.find(account.value);
    check(uitr != users.end(), "Not a Seeds user!");
}


// store a singleton with the cycle #
// each time calc start we compare cycle # and see if everything has finished, if yes we increase cycle
// if no we continue old cycle
// store cucle in harvest stat
// complete a cycle before going to a new one
// measure times 
void harvest::init_harvest_stat(name account) {
  auto the_time = eosio::current_time_point().sec_since_epoch();
  harveststat.emplace(_self, [&](auto& user) {
    user.account = account;
    
    user.transactions_score = 0;
    user.tx_timestamp = the_time;
    
    user.reputation_score = 0;
    user.rep_timestamp = the_time;

    user.planted_score = 0;
    user.planted_timestamp = the_time;

    user.contribution_score = 0;
    user.contrib_timestamp = the_time;

    user.community_building_score = 0;
    user.community_building_timestamp = the_time;
  });
}


void harvest::calcplanted() {
  require_auth(_self);

  auto users = balances.get_index<"byplanted"_n>();

  uint64_t users_number = std::distance(users.begin(), users.end()) - 1;

  uint64_t current_user = 0;

  auto uitr = users.begin();

  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  while (uitr != users.end()) {
    if (uitr->account != get_self()) {
      uint64_t score = (current_user * 100) / users_number;

      auto hitr = harveststat.find(uitr->account.value);

      if (hitr == harveststat.end()) {
          init_harvest_stat(uitr->account);
          hitr = harveststat.find(uitr->account.value);
      } 

      harveststat.modify(hitr, _self, [&](auto& user) {
        user.planted_score = score;
        user.planted_timestamp = now_time;
      });

      current_user++;
    }

    uitr++;
  }

  print("HARVEST: calcplanted executed");

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
  if (account == contracts::invites || account == contracts::onboarding) {
    return;
  }
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "harvest: no user");
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

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::transfer_action action{contracts::token, {contracts::bank, "active"_n}};
  action.send(contracts::bank, beneficiary, quantity, "");
}

void harvest::testreward(name from) {
  require_auth(get_self());
  init_balance(from);

  auto bitr = balances.find(from.value);

  balances.modify(bitr, _self, [&](auto& user) {
    user.reward = asset(100000, seeds_symbol);
  });
}

void harvest::testclaim(name from, uint64_t request_id, uint64_t sec_rewind) {
  require_auth(get_self());
  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();
  check(ritr != refunds.end(), "No refund found");

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      refunds.modify(ritr, _self, [&](auto& refund) {
        refund.request_time = eosio::current_time_point().sec_since_epoch() - sec_rewind;
      });
    }
    ritr++;
  }
  
}

void harvest::testupdatecs(name account, uint64_t contribution_score) {
  require_auth(get_self());
  uint64_t now_time = eosio::current_time_point().sec_since_epoch();
  auto hitr = harveststat.find(account.value);
  if (hitr == harveststat.end()) {
    harveststat.emplace(_self, [&](auto& user) {
      user.account = account;
      user.contribution_score = contribution_score;
      user.contrib_timestamp = now_time;
    });
  } else {
    harveststat.modify(hitr, _self, [&](auto& user) {
      user.contribution_score = contribution_score;
      user.contrib_timestamp = now_time;
    });
  }
}

  double harvest::get_rep_multiplier(name account) {
    auto ritr = rep.find(account.value);
    if (ritr == rep.end()) {
      return 0;
    }
    return utils::rep_multiplier_for_score(ritr->rank);

  }

void harvest::size_change(name id, int delta) {
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

void harvest::size_set(name id, uint64_t newsize) {
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

uint64_t harvest::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}
