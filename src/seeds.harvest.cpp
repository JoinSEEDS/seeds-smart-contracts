#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <seeds.harvest.hpp>

void harvest::reset() {
  require_auth(_self);

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
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
  size_set(tx_points_size, 0);

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }

  auto pitr = planted.begin();
  while (pitr != planted.end()) {
    pitr = planted.erase(pitr);
  }

  total.remove();

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

    add_planted(target, quantity);

    _deposit(quantity);
  }
}

void harvest::add_planted(name account, asset quantity) {
  auto bitr = balances.find(account.value);
  balances.modify(bitr, _self, [&](auto& user) {
    user.planted += quantity;
  });
  auto pitr = planted.find(account.value);
  if (pitr == planted.end()) {
    planted.emplace(_self, [&](auto& item) {
      item.account = account;
      item.planted = quantity;
      item.rank = 0;
    });
    size_change(planted_size, 1);
  } else {
    planted.modify(pitr, _self, [&](auto& item) {
      item.planted += quantity;
    });
  }
  
  change_total(true, quantity);

}

void harvest::sub_planted(name account, asset quantity) {
  auto fromitr = balances.find(account.value);
  check(fromitr->planted.amount >= quantity.amount, "not enough planted balance");
  balances.modify(fromitr, _self, [&](auto& user) {
    user.planted -= quantity;
  });

  auto pitr = planted.find(account.value);
  check(pitr != planted.end(), "user has no balance");
  if (pitr->planted.amount == quantity.amount) {
    planted.erase(pitr);
    size_change(planted_size, -1);
  } else {
    planted.modify(pitr, _self, [&](auto& item) {
      item.planted -= quantity;
    });
  }
  
  change_total(false, quantity);

}

void harvest::sow(name from, name to, asset quantity) {
    require_auth(from);
    check_user(from);
    check_user(to);

    init_balance(from);
    init_balance(to);

    sub_planted(from, quantity);
    add_planted(to, quantity);

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
    _withdraw(beneficiary, total);
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

        add_planted(from, ritr->amount);

        totalReplanted += ritr->amount.amount;

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

  sub_planted(from, quantity);

}

void harvest::runharvest() {
  require_auth(get_self());
}

ACTION harvest::updatetxpt(name account) {
  require_auth(get_self());
  calc_transaction_points(account);
}

ACTION harvest::updatecs(name account) {
  require_auth(account);
  calc_contribution_score(account);
}


// DEBUG action to clear scores tables
// Deploy before
ACTION harvest::clearscores() {
  require_auth(get_self());

  uint64_t limit = 200;

  auto titr = txpoints.begin();
  while (titr != txpoints.end() && limit > 0) {
    titr = txpoints.erase(titr);
    limit--;
  }  
  size_set(tx_points_size, 0);

  auto citr = cspoints.begin();
  while (citr != cspoints.end() && limit > 0) {
    citr = cspoints.erase(citr);
    limit--;
  }  


}

// copy everything to planted table

ACTION harvest::updtotal() { // remove when balances are retired
  require_auth(get_self());

  auto bitr = balances.find(_self.value);
  total_table tt = total.get_or_create(get_self(), total_table());
  tt.total_planted = bitr->planted;
  total.set(tt, get_self());
}

ACTION harvest::migrateplant(uint64_t startval) {
  require_auth(get_self());

  uint64_t limit = 100;

  auto bitr = startval == 0 ? balances.begin() : balances.find(startval);

  while (bitr != balances.end() && limit > 0) {
    if (bitr->planted.amount > 0 && bitr->account != _self) {
      auto pitr = planted.find(bitr->account.value);
      if (pitr == planted.end()) {
        planted.emplace(_self, [&](auto& entry) {
          entry.account = bitr->account;
          entry.planted = bitr->planted;
        });
        size_change(planted_size, 1);
      } else {
        planted.modify(pitr, _self, [&](auto& entry) {
          entry.account = bitr->account;
          entry.planted = bitr->planted;
        });
      }
    }
    bitr++;
    limit--;
  }

  if (bitr != balances.end()) {

    uint64_t next_value = bitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "migrateplant"_n,
        std::make_tuple(next_value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);

  } 
}

ACTION harvest::calctotal(uint64_t startval) {
  require_auth(get_self());

  uint64_t limit = 300;
  total_table tt = total.get_or_create(get_self(), total_table());
  if (startval == 0) {
    tt.total_planted = asset(0, seeds_symbol);
  }

  auto pitr = startval == 0 ? planted.begin() : planted.find(startval);

  while (pitr != planted.end() && limit > 0) {
    tt.total_planted += pitr->planted;
    pitr++;
    limit--;
  }
  total.set(tt, get_self());

  if (pitr != planted.end()) {

    uint64_t next_value = pitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calctotal"_n,
        std::make_tuple(next_value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);

  } 
}

// copy everything to harvest table
// ACTION harvest::updharvest(uint64_t startval) {

//   total_table tt = total.get_or_create(get_self(), total_table());
//   tt.total_planted = asset(0, seeds_symbol);
//   total.set(tt, get_self());

//   uint64_t limit = 50;

//   auto bitr = startval == 0 ? balances.begin() : balances.find(startval);

//   while (bitr != balances.end() && limit > 0) {
//     if (bitr->planted.amount > 0 && bitr->account != _self) {
//       planted.emplace(_self, [&](auto& entry) {
//         entry.account = bitr->account;
//         entry.planted = bitr->planted;
//       });
//       size_change(planted_size, 1);
//       change_total(true, bitr->planted);
//     }
//     bitr++;
//     limit--;
//   }

//   if (bitr != balances.end()) {

//     uint64_t next_value = bitr->account.value;
//     action next_execution(
//         permission_level{get_self(), "active"_n},
//         get_self(),
//         "updharvest"_n,
//         std::make_tuple(next_value)
//     );

//     transaction tx;
//     tx.actions.emplace_back(next_execution);
//     tx.delay_sec = 1;
//     tx.send(next_value, _self);

//   } 
// }

// Calculate Transaction Points for a single account
// Returns count of iterations
uint32_t harvest::calc_transaction_points(name account) {
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

  uint64_t  count = 0;
  uint64_t  limit = 200;
    
  //print("start " + account.to_string());

  while(tx_to_itr != transactions_by_to.rend() && count < limit) {

    if (tx_to_itr->timestamp < cutoffdate) {
      //print("date trigger ");

      // remove old transactions
      //tx_to_itr = transactions_by_to.erase(tx_to_itr);
      
      //auto it = transactions_by_to.erase(--tx_to_itr.base());// TODO add test for this
      //tx_to_itr = std::reverse_iterator(it);            
    } else {
      //print("update to ");

      // update "to"
      if (current_to != tx_to_itr->to) {
        current_to = tx_to_itr->to;
        current_num = 0;
        current_rep_multiplier = get_rep_multiplier(current_to);
      } else {
        current_num++;
      }

      //print("iterating over "+std::to_string(tx_to_itr->id));

      if (current_num < max_number_of_transactions) {
        uint64_t volume = tx_to_itr->quantity.amount;

      //print("volume "+std::to_string(volume));

        // limit max volume
        if (volume > max_quantity * 10000) {
              //print("max limit "+std::to_string(max_quantity * 10000));
          volume = max_quantity * 10000;
        }


        // multiply by receiver reputation
        double points = (double(volume) / 10000.0) * current_rep_multiplier;
        
        //print("tx points "+std::to_string(points));

        result += points;

      } 

    }
    tx_to_itr++;
    count++;
  }

  //print("set result "+std::to_string(result));

  // use ceil function so each schore is counted if it is > 0
    
  // DEBUG
  // if (result == 0) {
  //   result = 33.0;
  // }
  // enter into transaction points table
  auto titr = txpoints.find(account.value);
  uint64_t points = ceil(result);

  if (titr == txpoints.end()) {
    if (points > 0) {
      txpoints.emplace(_self, [&](auto& entry) {
        entry.account = account;
        entry.points = (uint64_t) ceil(result);
      });
      size_change(tx_points_size, 1);
    }
  } else {
    if (points > 0) {
      txpoints.modify(titr, _self, [&](auto& entry) {
        entry.points = points; 
      });
    } else {
      txpoints.erase(titr);
      size_change(tx_points_size, -1);
    }
  }

  return count;

}

void harvest::calctrxpts() {
    calctrxpt(0, 0, 400);
}

void harvest::calctrxpt(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  check(chunksize > 0, "chunk size must be > 0");

  uint64_t total = utils::get_users_size();
  auto uitr = start_val == 0 ? users.begin() : users.lower_bound(start_val);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    uint32_t num = calc_transaction_points(uitr->account);
    count += 1 + num;
    uitr++;
  }

  if (uitr == users.end()) {
    // done
  } else {
    uint64_t next_value = uitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calctrxpt"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void harvest::ranktxs() {
  ranktx(0, 0, 200);
}

void harvest::ranktx(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size(tx_points_size);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto txpt_by_points = txpoints.get_index<"bypoints"_n>();
  auto titr = start_val == 0 ? txpt_by_points.begin() : txpt_by_points.lower_bound(start_val);
  uint64_t count = 0;

  while (titr != txpt_by_points.end() && count < chunksize) {

    uint64_t rank = (current * 100) / total;

    //print(" rank: "+std::to_string(rank) + " total: " +std::to_string(total));


    txpt_by_points.modify(titr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    titr++;
  }

  if (titr == txpt_by_points.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = titr->by_points();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "ranktx"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
    
  }

}

void harvest::rankplanteds() {
  rankplanted(0, 0, 200);
}

void harvest::rankplanted(uint128_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size(planted_size);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto planted_by_planted = planted.get_index<"byplanted"_n>();
  auto pitr = start_val == 0 ? planted_by_planted.begin() : planted_by_planted.lower_bound(start_val);
  uint64_t count = 0;

  while (pitr != planted_by_planted.end() && count < chunksize) {

    uint64_t rank = (current * 100) / total;

    planted_by_planted.modify(pitr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    pitr++;
  }

  if (pitr == planted_by_planted.end()) {
    // Done.
  } else {
    // recursive call
    uint128_t next_value = pitr->by_planted();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankplanted"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(pitr->account.value, _self);
    
  }

}

void harvest::calccss() {
  calccs(0, 0, 100);
}

void harvest::calccs(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  check(chunksize > 0, "chunk size must be > 0");

  uint64_t total = utils::get_users_size();
  auto uitr = start_val == 0 ? users.begin() : users.lower_bound(start_val);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    calc_contribution_score(uitr->account);
    count++;
    uitr++;
  }

  if (uitr == users.end()) {
    // done
  } else {
    uint64_t next_value = uitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calccs"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

// [PS+RT+CB X Rep = Total Contribution Score]
void harvest::calc_contribution_score(name account) {
  uint64_t planted_score = 0;
  uint64_t transactions_score = 0;
  uint64_t community_building_score = 0;
  uint64_t reputation_score = 0;

  auto pitr = planted.find(account.value);
  if (pitr != planted.end()) planted_score = pitr->rank;

  auto titr = txpoints.find(account.value);
  if (titr != txpoints.end()) transactions_score = titr->rank;

  auto citr = cbs.find(account.value);
  if (citr != cbs.end()) community_building_score = citr->rank;

  auto ritr = rep.find(account.value);
  if (ritr != rep.end()) reputation_score = ritr->rank;

  uint64_t contribution_points = ( (planted_score + transactions_score + community_building_score) * reputation_score * 2) / 100; 

  auto csitr = cspoints.find(account.value);
  if (csitr == cspoints.end()) {
    if (contribution_points > 0) {
      cspoints.emplace(_self, [&](auto& item) {
        item.account = account;
        item.contribution_points = contribution_points;
      });
      size_change(cs_size, 1);
    }
  } else {
    if (contribution_points > 0) {
      cspoints.modify(csitr, _self, [&](auto& item) {
        item.contribution_points = contribution_points;
      });
    } else {
      cspoints.erase(csitr);
      size_change(cs_size, -1);
    }
  }
}

void harvest::rankcss() {
  rankcs(0, 0, 200);
}

void harvest::rankcs(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size(cs_size);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto cs_by_points = cspoints.get_index<"bycspoints"_n>();
  auto citr = start_val == 0 ? cs_by_points.begin() : cs_by_points.lower_bound(start_val);
  uint64_t count = 0;

  while (citr != cs_by_points.end() && count < chunksize) {

    uint64_t rank = (current * 100) / total;

    cs_by_points.modify(citr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    citr++;
  }

  if (citr == cs_by_points.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = citr->by_cs_points();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankcs"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
    
  }


}

void harvest::payforcpu(name account) {
    require_auth(get_self()); // satisfied by payforcpu permission
    require_auth(account);

    auto uitr = users.find(account.value);
    check(uitr != users.end(), "Not a Seeds user!");
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

void harvest::_deposit(asset quantity)
{
  check_asset(quantity);

  token::transfer_action action{contracts::token, {_self, "active"_n}};
  action.send(_self, contracts::bank, quantity, "");
}

void harvest::_withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::transfer_action action{contracts::token, {contracts::bank, "active"_n}};
  action.send(contracts::bank, beneficiary, quantity, "");
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
  auto csitr = cspoints.find(account.value);
  if (csitr == cspoints.end()) {
    if (contribution_score > 0) {
      cspoints.emplace(_self, [&](auto& item) {
        item.account = account;
        item.rank = contribution_score;
      });
      size_change(cs_size, 1);
    }
  } else {
    if (contribution_score > 0) {
      cspoints.modify(csitr, _self, [&](auto& item) {
        item.rank = contribution_score;
      });
    } else {
      cspoints.erase(csitr);
      size_change(cs_size, -1);
    }
  }
}

  double harvest::get_rep_multiplier(name account) {
    //return 1.0;  // DEBUg FOR TESTINg otherwise everyone on testnet has 0

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

void harvest::change_total(bool add, asset quantity) {
  total_table tt = total.get_or_create(get_self(), total_table());
  if (tt.total_planted.amount == 0) {
    tt.total_planted = asset(0, seeds_symbol);
  }
  if (add) {
    tt.total_planted = tt.total_planted + quantity;
  } else {
    tt.total_planted = tt.total_planted - quantity;
  }
  total.set(tt, get_self());
}
