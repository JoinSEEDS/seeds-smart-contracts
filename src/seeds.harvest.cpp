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

void harvest::calcrep() {
  require_auth(_self);

  auto usersrep = users.get_index<"byreputation"_n>();

  auto users_number = std::distance(usersrep.begin(), usersrep.end());

  uint64_t current_user = 0;

  auto uitr = usersrep.begin();

  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  while (uitr != usersrep.end()) {

    uint64_t score = (current_user * 100) / users_number;

    if (uitr->reputation == 0) {
      score = 0;
    }

    auto hitr = harveststat.find(uitr->account.value);

    if (hitr == harveststat.end()) {
      init_harvest_stat(uitr->account);
      hitr = harveststat.find(uitr->account.value);
    }

    harveststat.modify(hitr, _self, [&](auto& user) {
        user.reputation_score = score;
        user.rep_timestamp = now_time;
    });

    current_user++;
    uitr++;
  }

  print("HARVEST: calcrep executed");
}

// Calculate Transaction Points for a single account
void harvest::calc_tx_points(name account, uint64_t cycle) {

  action(
    permission_level{contracts::history, "active"_n},
    contracts::history, 
    "clearoldtrx"_n,
    std::make_tuple(account)
  ).send();

  auto three_moon_cycles = moon_cycle * 3;
  auto now = eosio::current_time_point().sec_since_epoch();
  auto cutoffdate = now - three_moon_cycles;

  // get all transactions for this account
  transaction_tables transactions(contracts::history, account.value);

  auto transactions_by_to = transactions.get_index<"byto"_n>();
  auto tx_to_itr = transactions_by_to.begin();

  double result = 0;

  uint64_t max_quantity = 1777; // get this from settings // TODO verify this number
  uint64_t max_number_of_transactions = 26;

  name      current_to = name("");
  uint64_t  current_num = 0;
  double    current_rep_multiplier = 0.0;

  while(tx_to_itr != transactions_by_to.end()) {

    if (tx_to_itr->timestamp < cutoffdate) {

      // remove old transactions
      //tx_to_itr = transactions_by_to.erase(tx_to_itr);
      tx_to_itr++;

    } else {

      // update "to"
      if (current_to != tx_to_itr->to) {
        current_to = tx_to_itr->to;
        current_num = 0;
        current_rep_multiplier = utils::get_rep_multiplier(current_to);
      } else {
        current_num++;
      }

      if (current_num < max_number_of_transactions && current_rep_multiplier > 0.00001) {
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
    
  // enter into transaction points table
  auto hitr = txpoints.find(account.value);
  if (hitr == txpoints.end()) {
    txpoints.emplace(_self, [&](auto& entry) {
      entry.account = account;
      entry.points = (uint64_t) ceil(result);
      entry.cycle = cycle;
    });
  } else {
    txpoints.modify(hitr, _self, [&](auto& entry) {
      entry.points = (uint64_t) ceil(result);
      entry.cycle = cycle;
    });
  }

}

uint64_t harvest::_calctrxpt(uint64_t start_index, uint64_t increment) {
  require_auth(_self);

  auto uitr = start_index == 0 ? users.begin() : users.find(start_index);

  uint64_t count = 0; 

  string debugstring = "xx start ix: "+ std::to_string(start_index);

  uint64_t current_user = 0;

  while (uitr != users.end() && count < increment) {
    debugstring = debugstring +  " | " +  std::to_string(start_index + count) + ": acct: "+uitr->account.to_string();

    calc_tx_points(uitr->account, 0);
    count++;
    uitr++;
  }

  print(debugstring);

  return uitr == users.end() ? 0 : uitr->account.value;

}



const name harvest_table_size = "size.hrvst"_n; 
const name trxpt_1_size       = "size.trx.1"_n; 
const name planted_1_size     = "size.plnt.1"_n;
const name cbs_1_size         = "size.cbs.1"_n;
const name rep_1_size         = "size.rep.1"_n;


uint64_t harvest::_cget(name key) {
  auto citr = cycle.find(key.value);
  if (citr != cycle.end()) {
    return citr->value;
  } else {
    return 0;
  }
}

void harvest::cset(name key, uint64_t value) {
  auto citr = cycle.find(key.value);
  if (citr == cycle.end()) {
    cycle.emplace(_self, [&](auto& item) {
      item.cycle_id = key;
      item.value = value;
    });
  } else {
    cycle.modify(citr, _self, [&](auto& item) {
      item.value = value;
    });
  }
}

void harvest::_cincrement(name key) {
  _cincby(key, 1);
}

void harvest::_cdecrement(name key) {
  _cincby(key, -1);
}

void harvest::_cincby(name key, int by) {
  auto citr = cycle.find(key.value);
  if (citr == cycle.end()) {
    cycle.emplace(_self, [&](auto& item) {
      item.cycle_id = key;
      item.value = 0;
    });
  } else {
    if (by > 0 || citr->value >= -by) {
      cycle.modify(citr, _self, [&](auto& item) {
        item.value = item.value + by;
      });
    }
  }
}


uint64_t harvest::calctrx(uint64_t start_index, uint64_t increment) {
  require_auth(_self);

  auto users = txpoints.get_index<"bypoints"_n>();
  
  //uint64_t counted_users_number = std::distance(users.lower_bound(1), users.end());

  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  uint64_t index = get_cycle_index(calctrx_cycle);
  uint64_t length = std::distance(users.begin(), users.end());
  uint64_t number_of_users_with_zero_score = length - counted_users_number;

  if (index >= length) {
    index = 0;
  } else  if (index < number_of_users_with_zero_score) {
    index = number_of_users_with_zero_score;
  }

  auto uitr = users.begin();
  std::advance(uitr, index);

  uint64_t current_index = index; 
  
  uint64_t end_index = index + 100;

  while (uitr != users.end() && current_index < end_index) {
    uint64_t score;

    if (uitr->points == 0) {
      score = 0;
    } else {
      score = ( (current_index - number_of_users_with_zero_score) * 100) / counted_users_number;
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

    current_index++;

    uitr++;
  }

  set_cycle_index(calctrx_cycle, uitr == users.end() ? 0 : end_index);

}

void harvest::calccbs() {
  require_auth(_self);

  auto users = cbs.get_index<"bycbs"_n>();

  uint64_t users_number = std::distance(users.begin(), users.end());

  uint64_t current_user = 0;
  uint64_t now_time = eosio::current_time_point().sec_since_epoch();

  auto uitr = users.begin();

  while (uitr != users.end()) {
    uint64_t score = (current_user * 100) / users_number;

    if (uitr->community_building_score == 0) {
      score = 0;
    }

    auto hitr = harveststat.find(uitr->account.value);

    if (hitr == harveststat.end()) {
        init_harvest_stat(uitr->account);
        hitr = harveststat.find(uitr->account.value);
    } 

    harveststat.modify(hitr, _self, [&](auto& user) {
      user.community_building_score = score;
      user.community_building_timestamp = now_time;
    });

    current_user++;

    uitr++;
  }


}

// [PS+RT+CB X Rep = Total Contribution Score]
void harvest::calccs() {

  require_auth(_self);

  auto hitr = harveststat.begin();
  while (hitr != harveststat.end()) {

    double rep = utils::rep_multiplier_for_score(hitr->reputation_score);

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

uint64_t harvest::get_cycle_index(name cycle_id) {
  auto citr = cycle.find(cycle_id.value);
  if (citr != cycle.end()) {
    return citr->value;
  } else {
    return 0;
  }
} 

void harvest::set_cycle_index(name cycle_id, uint64_t index) {
  auto citr = cycle.find(cycle_id.value);
  if (citr == cycle.end()) {
    cycle.emplace(_self, [&](auto& item) {
      item.cycle_id = cycle_id;
      item.value = index;
    });
  } else {
    cycle.modify(citr, _self, [&](auto& item) {
      item.value = index;
    });
  }
}

void harvest::caclharvest() {
    uint64_t increment = config.get("batchsize.h"_n.value, "settgs.seeds::config: the batchsize.h parameter has not been initialized").value;
    caclharvestn(increment);
}

void harvest::caclharvestn(uint64_t increment) {

  require_auth(get_self());

  name harvest_cycle = "harvest.1"_n;
  
  uint64_t start_value = get_cycle_index(harvest_cycle);

  //uint64_t start_type = get_cycle_index("type"_n);
    
  auto uitr = start_value == 0 ? users.begin() : users.find(start_index);

  uint64_t count = 0; 

  //string debugstring = " start ix: "+ std::to_string(start_index);

  while (uitr != users.end() && count < increment) {

    //debugstring = debugstring +  " | " +  std::to_string(start_index + count) + ": acct: "+uitr->account.to_string();

    calcscore(uitr->account);
    count++;
    uitr++;
  }

  //print(debugstring);

  set_cycle_index(harvest_cycle, uitr == users.end() ? 0 : uitr->account.value);


}

void harvest::caclharvestm(uint64_t increment) {

  require_auth(get_self());

  check(increment > 0, "increment must be > 0");

  name harvest_cycle = "harvest.1"_n;

  // keys for the cycle table
  name calctrxpt_cycle = "calctrxpt"_n;
  name calctrx_cycle = "calctrx"_n;
  name calcrep_cycle = "calcrep"_n;
  name calcplanted_cycle = "calcplanted"_n;
  name calccbs_cycle = "calccbs"_n;
  name calccs_cycle = "calccs"_n;

  uint64_t start_value = get_cycle_index(harvest_cycle);

  uint64_t start_type = get_cycle_index("type"_n);
    
  uint64_t next_value = 0;

  uint64_t next_type = start_type;

  switch (start_type)
  {
  case calctrxpt_cycle.value:
    next_value = _calctrxpt(start_value, increment);
    if (next_value == 0) next_type = calctrx_cycle.value;
    break;

  case calctrx_cycle.value:
    next_value = _calctrx(start_value, increment);
    if (next_value == 0) next_type = calcrep_cycle.value;
    break;
  
  case calcrep_cycle.value:
    next_value = _calcrep(start_value, increment);
    if (next_value == 0) next_type = calcplanted_cycle.value;
    break;

  case calcplanted_cycle.value:
    next_value = _calcplanted(start_value, increment);
    if (next_value == 0) next_type = calccbs_cycle.value;
    break;

  case calccbs_cycle.value:
    next_value = _calccbs(start_value, increment);
    if (next_value == 0) next_type = calccs_cycle.value;
    break;

  case calccs_cycle.value:
    next_value = _calc_cs(start_value, increment);
    if (next_value == 0) next_type = calctrxpt_cycle.value;
    break;

  default:
    next_value = 0;
    next_type = calctrxpt_cycle.value
    break;
  }

  set_cycle_index(harvest_cycle, uitr == users.end() ? 0 : uitr->account.value);
  set_cycle_index("type"_n, next_type);


}


void harvest::calcscore(name account) {
  require_auth(get_self());

  const name organization = "organisation"_n;

  auto user = users.get(account.value, "error: no account!");

  if (user.type == organization) {
    _scoreorg(user.account);
  } else {
    _scoreuser(user.account, user.reputation);
  }
  
}
void harvest::_scoreuser(name account, uint64_t reputation) {

  // calc_tx_points(account, 0);

  // uint64_t transactions_score = _calc_tx_score(account);

  // uint64_t reputation_score = _calc_reputation_score(account, reputation);

  // uint64_t community_building_score = _calc_cb_score(account);

  uint64_t planted_score = _calc_planted_score(account);

  //print("CS..");

  // double rep = utils::rep_multiplier_for_score(reputation_score);

  // uint64_t contribution_score = uint64_t((planted_score + transactions_score + community_building_score) * rep);

  auto hitr = harveststat.find(account.value);

  if (hitr == harveststat.end()) {
      init_harvest_stat(account);
      hitr = harveststat.find(account.value);
  } 

  // string s = "planted_score: " + std::to_string(planted_score) +
  //       " transactions_score: " + std::to_string(transactions_score) +
  //       " reputation_score: " + std::to_string(reputation_score) +
  //       " community_building_score: " + std::to_string(community_building_score) +
  //       " contribution_score: " + std::to_string(contribution_score);

  //print(s);

/* // TEST MODE - not writing to the chain just yet - I want to test performance on the live data.
  harveststat.modify(hitr, _self, [&](auto& user) {
    user.planted_score = planted_score;
    user.transactions_score = transactions_score;
    user.reputation_score = reputation_score;
    user.community_building_score = community_building_score;
    user.contribution_score = contribution_score;
  });
*/

}

uint64_t harvest::_calc_tx_score(name account) {

  auto txitr = txpoints.find(account.value);
  uint64_t tx_score = 0;

  if (txitr != txpoints.end()) {
    uint64_t points = txitr->points;
    if (points >= 1) {
      auto txpoints_by_points = txpoints.get_index<"bypoints"_n>();
      auto ptitr = txpoints_by_points.find(points);
      auto lower_bound_1_itr = txpoints_by_points.lower_bound(1);
      if (lower_bound_1_itr != txpoints_by_points.end()) {
        uint64_t counted_number = std::distance(lower_bound_1_itr, txpoints_by_points.end());
        uint64_t counted_index = std::distance(lower_bound_1_itr, ptitr);
        tx_score = (counted_index * 100) / counted_number; // ==> 0 - 99    
      } 
    }
  }
  return tx_score;
}

uint64_t harvest::_calc_reputation_score(name account, uint64_t reputation) {
    uint64_t rep_score = 0;

  if (reputation != 0) {
    auto users_by_rep = users.get_index<"byreputation"_n>();
    auto repitr = users_by_rep.find(reputation);
    check(repitr != users_by_rep.end(), "rep not found");
    auto lower_bound_1_itr = users_by_rep.lower_bound(1);
    if (lower_bound_1_itr != users_by_rep.end()) {
      uint64_t counted_number = std::distance(lower_bound_1_itr, users_by_rep.end());
      uint64_t counted_index = std::distance(lower_bound_1_itr, repitr);
      rep_score = (counted_index * 100) / counted_number; 
    } 
  }

  return rep_score;
}

uint64_t harvest::_calc_cb_score(name account) {
  auto citr = cbs.find(account.value);
  uint64_t cb_score = 0;
  if (citr != cbs.end()) {
    uint64_t cb_points = citr->community_building_score;
    auto users_by_cbs = cbs.get_index<"bycbs"_n>();
    auto cbs_itr = users_by_cbs.find(cb_points);
    auto lower_bound_1_itr = users_by_cbs.lower_bound(1);
    if (lower_bound_1_itr != users_by_cbs.end()) {
      uint64_t counted_number = std::distance(lower_bound_1_itr, users_by_cbs.end());
      uint64_t counted_index = std::distance(lower_bound_1_itr, cbs_itr);
      cb_score = (counted_index * 100) / counted_number; 
    } 
  }
  return cb_score;
}

uint64_t harvest::_calc_planted_score(name account) {
  auto pitr = balances.find(account.value);
  uint64_t planted_score = 0;
  if (pitr != balances.end()) {
    uint64_t planted_amount = pitr->planted.amount;
    auto users_by_planted = balances.get_index<"byplanted"_n>();
    
    auto by_planted_itr = users_by_planted.find(planted_amount);

    auto lower_bound_1_itr = users_by_planted.lower_bound(1);
    if (lower_bound_1_itr != users_by_planted.end()) {
      uint64_t counted_number = std::distance(lower_bound_1_itr, users_by_planted.end());
      uint64_t counted_index = std::distance(lower_bound_1_itr, by_planted_itr);
      planted_score = (counted_index * 100) / counted_number; 
    } 
  }
  return planted_score;
}



void harvest::_scoreorg(name orgname) {
  
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

void harvest::testsetrs(name account, uint64_t value) {
  require_auth(get_self());
  auto hitr = harveststat.find(account.value);
  if (hitr == harveststat.end()) {
    init_harvest_stat(account);
    hitr = harveststat.find(account.value);
  }

  harveststat.modify(hitr, _self, [&](auto& item) {
    item.reputation_score = value;
  });

}


