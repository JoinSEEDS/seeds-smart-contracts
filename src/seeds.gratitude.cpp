#include <seeds.gratitude.hpp>


ACTION gratitude::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
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

  auto actr = acks.find(to.value);
  if (actr == acks.end()) {
    acks.emplace(_self, [&](auto& item) {
      item.receiver = to;
      item.donors = vector{from};
    });
  } else {
    acks.modify(actr, get_self(), [&](auto &item) {
        item.donors.push_back(from);
    });
  }
}


ACTION gratitude::newround() {
  require_auth(get_self());
  auto generated_gratz = config_get(gratzgen);

  auto contract_balance = eosio::token::get_balance(contracts::token, get_self(), seeds_symbol.code());
  uint64_t tot_accounts = get_size("balances.sz"_n);
  uint64_t volume = get_current_volume();

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    uint64_t my_received = bitr->received.amount;
    balances.modify(bitr, _self, [&](auto& item) {
        item.received = asset(0, gratitude_symbol);
        item.remaining = asset(generated_gratz, gratitude_symbol);
    });
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

  auto generated_gratz = config_get(gratzgen);

  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto & item){
      item.account = account;
      item.received = asset(0, gratitude_symbol);
      item.remaining = asset(generated_gratz, gratitude_symbol);
    });
    size_change("balances.sz"_n, 1);
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