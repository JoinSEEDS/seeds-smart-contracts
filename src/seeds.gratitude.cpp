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

  // setup first round
  stats.emplace(_self, [&](auto& item) {
    item.round_id = 1;
    item.num_transfers = 0;
    item.volume = asset(0, gratitude_symbol);
  });

}

ACTION gratitude::give (name from, name to, asset quantity, string memo) {
  require_auth(from);

  check( from != to, "gratitude: cannot give to self" );
  check( is_account( to ), "gratitude: to account does not exist");
  check_asset(quantity);
  check( quantity.amount > 0, "gratitude: must give positive quantity" );

  // Should create balances if not there yet
  init_balances(to);
  init_balances(from);

  sub_gratitude(from, quantity);
  add_gratitude(to, quantity);

  update_stats(from, to, quantity);
}

ACTION gratitude::acknowledge (name from, name to, string memo) {
  require_auth(from);

  check( from != to, "gratitude: cannot give to self" );
  check( is_account( to ), "gratitude: to account does not exist");

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
}

ACTION gratitude::newround() {
  require_auth(get_self());

  auto contract_balance = eosio::token::get_balance(contracts::token, get_self(), seeds_symbol.code());
  uint64_t tot_accounts = get_size("balances.sz"_n);
  uint64_t volume = get_current_volume();

  auto actr = acks.begin();
  while (actr != acks.end()) {
    calc_acks(actr->donor);
    actr = acks.erase(actr);
  }

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    uint64_t my_received = bitr->received.amount;
    // reset gratitude for account
    init_balances(bitr->account);
    float split_factor = my_received / (float)volume;
    uint64_t payout = contract_balance.amount * split_factor;
    // Pay out SEEDS in store
    if (payout > 0) _transfer(bitr->account, asset(payout, seeds_symbol), "gratitude bonus");
    bitr++;
  }
}

/// ----------================ PRIVATE ================----------

void gratitude::check_user (name account) {
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "gratitude: user not found");
}

void gratitude::calc_acks (name donor) {
  auto bitr = balances.find(donor.value);
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
  auto stitr = stats.rbegin();
  // Should always work because reset always creates first round
  return stitr->volume.amount;
}

void gratitude::update_stats(name from, name to, asset quantity) {
  auto stitr = stats.rbegin();
  auto round_id = stitr->round_id;

  auto stitr2 = stats.find(round_id);
  auto oldvolume = stitr2->volume.amount;
  auto oldtransfers = stitr2->num_transfers;
  stats.modify(stitr2, _self, [&](auto& item) {
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