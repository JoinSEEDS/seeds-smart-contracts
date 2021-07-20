#include <seeds.gratitude.hpp>


ACTION gratitude::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }

  auto actr = acks.begin();
  while (actr != acks.end()) {
    actr = acks.erase(actr);
  }

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }

  auto stitr = stats.begin();
  while (stitr != stats.end()) {
    stitr = stats.erase(stitr);
  }

  auto stitr2 = stats2.begin();
  while (stitr2 != stats2.end()) {
    stitr2 = stats2.erase(stitr2);
  }

  // setup first round
  stats2.emplace(_self, [&](auto& item) {
    item.round_id = 1;
    item.num_transfers = 0;
    item.num_acks = 0;
    item.volume = asset(0, gratitude_symbol);
    item.round_pot = asset(0, seeds_symbol);
  });

}

ACTION gratitude::migratestats() {
  require_auth(get_self());

  auto sitr = stats.begin();
  while(sitr != stats.end()) {
    stats2.emplace(get_self(), [&](auto& item) {
      item.round_id = sitr->round_id;
      item.num_transfers = sitr->num_transfers;
      item.volume = sitr->volume;
      // new values
      item.num_acks = 0;
      item.round_pot = asset(0, seeds_symbol);
    });
    // Remove old one
    sitr = stats.erase(sitr);
  }
}

ACTION gratitude::give (name from, name to, asset quantity, string memo) {
  require_auth(from);

  check(from != to, "gratitude: cannot give to self");
  check(is_account(to), "gratitude: to account does not exist");
  check_user(to);
  check_asset(quantity);
  check(quantity.amount > 0, "gratitude: must give positive quantity");

  // Should create balances if not there yet
  init_balances(to);
  init_balances(from);

  sub_gratitude(from, quantity);
  add_gratitude(to, quantity);

  update_stats(from, to, quantity);
}

ACTION gratitude::acknowledge (name from, name to, string memo) {
  require_auth(from);

  check(from != to, "gratitude: cannot give to self");
  check(is_account(to), "gratitude: to account does not exist");
  check_user(to);

  // Should create balances if not there yet
  init_balances(to);
  init_balances(from);

  auto actr = acks.find(from.value);
  if (actr == acks.end()) {
    acks.emplace(_self, [&](auto& item) {
      item.donor = from;
      item.receivers = vector{to};
    });
  } else {
    acks.modify(actr, get_self(), [&](auto &item) {
        item.receivers.push_back(to);
    });
  }

  // Updates ack stats
  auto stitr = stats2.rbegin();
  auto round_id = stitr->round_id;

  // Have to use find to get a modifiable operator
  auto stitr2 = stats2.find(round_id);
  auto oldacks = stitr2->num_acks;
  stats2.modify(stitr2, _self, [&](auto& item) {
      item.num_acks = oldacks + 1;
  });
}


ACTION gratitude::testacks() {
  require_auth(get_self());

  auto actr = acks.begin();

  while (actr != acks.end()) {
    _calc_acks(actr->donor);
    actr = acks.erase(actr);
  }
}

ACTION gratitude::calcacks(uint64_t start) {
  require_auth(get_self());

  auto actr = start == 0 ? acks.begin() : acks.find(start);

  if (actr != acks.end()) {
    _calc_acks(actr->donor);
    actr = acks.erase(actr);
  }
  // If there is still more, do recursion call
  if (actr != acks.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "calcacks"_n,
      std::make_tuple(actr->donor.value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(actr->donor.value, _self);
  } else {
    // Otherwise, starts recursive payout
    auto contract_balance = eosio::token::get_balance(contracts::token, get_self(), seeds_symbol.code());
    float potkeep = config_get(gratz_potkp) / (float)100;
    uint64_t usable_amount = contract_balance.amount - (contract_balance.amount * potkeep);

    action(
      permission_level{get_self(), "active"_n},
      get_self(),
      "payround"_n,
      std::make_tuple((uint64_t)0, usable_amount)
    ).send();
  }
}

// Recursively pay accounts
ACTION gratitude::payround(uint64_t start, uint64_t usable_bal) {
  require_auth(get_self());

  auto bitr = start == 0 ? balances.begin() : balances.find(start);
  uint64_t current = 0;
  auto chunksize = config_get("batchsize"_n);

  while (current < chunksize) {
    if (bitr != balances.end()) {
      uint64_t volume = get_current_volume();
      uint64_t my_received = bitr->received.amount;
      // reset gratitude for account
      reset_balances(bitr->account);
      float split_factor = my_received / (float)volume;
      uint64_t payout = usable_bal * split_factor;
      if (payout > 0) _transfer(bitr->account, asset(payout, seeds_symbol), "gratitude bonus");
      bitr++;
      current++;
    } else break;
  }

  // if there's more
  if (bitr != balances.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "payround"_n,
      std::make_tuple(bitr->account.value, usable_bal)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(bitr->account.value, _self);
  }
  // Else, after all payouts are complete
  else {
    // adds a new round
    auto stitr = stats2.rbegin();
    auto cur_round_id = stitr->round_id;
    auto oldpot = stitr->round_pot;
    float potkeep = config_get(gratz_potkp) / (float)100;
    uint64_t newpot = oldpot.amount * potkeep;

    stats2.emplace(_self, [&](auto& item) {
      item.round_id = ++cur_round_id;
      item.num_transfers = 0;
      item.num_acks = 0;
      item.volume = asset(0, gratitude_symbol);
      item.round_pot = asset(newpot, seeds_symbol);
    });
  }
}

ACTION gratitude::newround() {
  require_auth(get_self());

  action(
    permission_level{get_self(), "active"_n},
    get_self(),
    "calcacks"_n,    
    std::make_tuple((uint64_t)0)
  ).send();
}


// Receive incoming SEEDS
ACTION gratitude::deposit (name from, name to, asset quantity, string memo) {
  require_auth(from);

  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
      to  ==  get_self() &&                     // to here
      quantity.symbol == seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);

    // Updates stats
    auto stitr = stats2.rbegin();
    auto round_id = stitr->round_id;

    // Have to use find to get a modifiable operator
    auto stitr2 = stats2.find(round_id);
    auto oldpot = stitr2->round_pot.amount;
    stats2.modify(stitr2, _self, [&](auto& item) {
        item.round_pot = asset(oldpot + quantity.amount, seeds_symbol);
    });
  }

}

/// ----------================ PRIVATE ================----------

void gratitude::check_user (name account) {
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "gratitude: user not found");
}

void gratitude::_calc_acks (name donor) {
  auto bitr = balances.find(donor.value);
  if (bitr == balances.end()) {
    init_balances(donor);
    bitr = balances.find(donor.value);
  }

  uint64_t remaining = bitr->remaining.amount;

  auto min_acks = config_get(gratz_acks);

  auto actr = acks.find(donor.value);
  if (actr != acks.end()) {
    std::map<name, uint64_t> unique_recs;
    for (std::size_t i = 0; i < actr->receivers.size(); i++) {
      unique_recs[actr->receivers[i]]++;
    }
    auto uritr = unique_recs.begin();
    while (uritr != unique_recs.end()) {
      uint64_t received = 0;
      if (actr->receivers.size() < min_acks) {
        received = (remaining / min_acks) * uritr->second;
      } else {
        received = (remaining / actr->receivers.size()) * uritr->second;
      }

      sub_gratitude(donor, asset(received, gratitude_symbol));
      add_gratitude(uritr->first, asset(received, gratitude_symbol));
      update_stats(donor, uritr->first, asset(received, gratitude_symbol));
      uritr++;
    }
  }
}

uint64_t gratitude::get_current_volume() {
  auto stitr = stats2.rbegin();
  // Should always work because reset always creates first round
  return stitr->volume.amount;
}

void gratitude::update_stats(name from, name to, asset quantity) {
  // Updating the last stats item
  auto itr = stats2.rbegin();
  auto round_id = itr->round_id;

  auto stitr = stats2.find(round_id);
  auto oldvolume = stitr->volume.amount;
  auto oldtransfers = stitr->num_transfers;
  stats2.modify(stitr, _self, [&](auto& item) {
    item.volume = asset(oldvolume + quantity.amount, gratitude_symbol);
    item.num_transfers = oldtransfers + 1;
  });
}

void gratitude::init_balances (name account) {
  auto bitr = balances.find(account.value);

  uint64_t generated_gratz = 0;
  auto uitr = users.find(account.value);
  if (uitr != users.end()) {
    switch (uitr->status)
    {
    case "citizen"_n:
      generated_gratz = config_get(gratzgen_cit);
      break;
    
    case "resident"_n:
      generated_gratz = config_get(gratzgen_res);
      break;
    }
  }

  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto & item){
      item.account = account;
      item.received = asset(0, gratitude_symbol);
      item.remaining = asset(generated_gratz, gratitude_symbol);
    });
    size_change("balances.sz"_n, 1);
  }
}

void gratitude::reset_balances (name account) {
  auto bitr = balances.find(account.value);

  uint64_t generated_gratz = 0;
  auto uitr = users.find(account.value);
  if (uitr != users.end()) {
    switch (uitr->status)
    {
    case "citizen"_n:
      generated_gratz = config_get(gratzgen_cit);
      break;
    
    case "resident"_n:
      generated_gratz = config_get(gratzgen_res);
      break;
    }
  }

  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto & item){
      item.account = account;
      item.received = asset(0, gratitude_symbol);
      item.remaining = asset(generated_gratz, gratitude_symbol);
    });
    size_change("balances.sz"_n, 1);
  } else {
    balances.modify(bitr, _self, [&](auto& item) {
        item.received = asset(0, gratitude_symbol);
        item.remaining = asset(generated_gratz, gratitude_symbol);
    });
  }
}


void gratitude::add_gratitude (name account, asset quantity) {
  check_asset(quantity);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "gratitude: no balance object found for " + account.to_string());

  balances.modify(bitr, _self, [&](auto & item){
    item.received += quantity;
  });

}

void gratitude::sub_gratitude (name account, asset quantity) {
  check_asset(quantity);

  auto bitr = balances.find(account.value);
  check(bitr != balances.end(), "gratitude: no balances object found for " + account.to_string());
  check(bitr -> remaining.amount >= quantity.amount, "gratitude: not enough gratitude to give");

  balances.modify(bitr, _self, [&](auto & item){
    item.remaining -= quantity;
  });
}

void gratitude::size_change(name id, int delta) {
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

void gratitude::size_set(name id, uint64_t newsize) {
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

uint64_t gratitude::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}


uint64_t gratitude::config_get(name key) {
  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}

// Transfers out stored SEEDS
void gratitude::_transfer (name beneficiary, asset quantity, string memo) {
  utils::check_asset(quantity);
  token::transfer_action action{contracts::token, {contracts::gratitude, "active"_n}};
  action.send(contracts::gratitude, beneficiary, quantity, memo);
}