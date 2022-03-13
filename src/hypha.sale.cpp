#include <hypha.sale.hpp>

void sale::reset() {
  require_auth(_self);

  config.remove();

  // legacy code
  // asset citizen_limit =  asset(uint64_t(2500000000), seeds_symbol);
  // asset resident_limit =  asset(uint64_t(2500000000), seeds_symbol);
  // asset visitor_limit =  asset(26000 * 10000, seeds_symbol);
  // updatelimit(citizen_limit, resident_limit, visitor_limit);

  asset tlos_per_usd =  asset(0.03 * 10000, tlos_symbol);

  updatetlos(tlos_per_usd);

  unpause();
  setflag(tlos_paused_flag, 1);

  // check(false, "Comment this out- safety stop. Always check in uncommented. ");
  
  sold.remove();

  auto pitr = payhistory.begin();
  while(pitr != payhistory.end()) {
    pitr = payhistory.erase(pitr);
  }
  
  auto ritr = rounds.begin();
  while(ritr != rounds.end()){
    ritr = rounds.erase(ritr);
  }

  auto phitr = pricehistory.begin();
  while(phitr != pricehistory.end()) {
    phitr = pricehistory.erase(phitr);
  }

  auto fitr = flags.begin();
  while(fitr != flags.end()) {
    fitr = flags.erase(fitr);
  }

}

asset sale::token_for_usd(asset usd_quantity, asset token) {
  update_price();

  soldtable s = sold.get_or_create(get_self(), soldtable());
 
  double usd_total = double(usd_quantity.amount);
  double usd_remaining = usd_total;
  double token_amount = 0.0;

  auto ritr = rounds.begin(); //rounds.find(p -> current_round);

  uint64_t round_start_volume = 0;

  while(ritr != rounds.end() && usd_remaining > 0) {
    uint64_t round_end_volume = ritr->max_sold;

    if (s.total_sold < round_end_volume) {
      double usd_per_token = asset_factor_d(ritr->token_per_usd) / double(ritr->token_per_usd.amount);

      // num available
      double available_in_round = round_end_volume - std::max(round_start_volume, s.total_sold);

      // price of available tokens
      double usd_available = available_in_round * usd_per_token;

      // if < usd amount remaining -> calculate, add, and exit
      if (usd_available >= usd_remaining) {
        token_amount += (usd_remaining * ritr->token_per_usd.amount) / asset_factor(ritr->token_per_usd);
        usd_remaining = 0;
        break;
      } else {
        usd_remaining -= usd_available;
        token_amount += available_in_round;
      }
    }
    
    round_start_volume = round_end_volume;
    ritr++;

    double asset_factor = asset_factor_d(usd_quantity);

    check(ritr != rounds.end(), "not enough funds available. requested USD value: "+std::to_string(usd_total/asset_factor) + 
    " available USD value: "+std::to_string( (usd_total - usd_remaining) / asset_factor) + " max vol: " + std::to_string(round_end_volume));

  }

  return asset(token_amount, seeds_symbol);
}

void sale::purchase_usd(name buyer, asset usd_quantity, string paymentSymbol, string memo) {
  check(!is_paused(), "Contract is paused - no purchase possible.");

  check(usd_quantity.symbol.precision() == 4, "expected precision 4 for USD");

  eosio::multi_index<"users"_n, tables::user_table> users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(buyer.value);
  check(uitr != users.end(), "not a seeds user " + buyer.to_string());

  configtable c = config.get();

  asset token_per_usd = c.token_per_usd;
  uint64_t tokens_purchased = 0;

  asset seeds_limit;
  switch (uitr->status) {
    case "citizen"_n:
      seeds_limit = c.citizen_limit;
      break;
    case "resident"_n:
      seeds_limit = c.resident_limit;
      break;
    case "visitor"_n:
      seeds_limit = c.visitor_limit;
      break;
    case "inactive"_n:
      seeds_limit = c.visitor_limit;
      break;
  }
  
  auto token_symbol = hypha_symbol;
  asset token_asset = asset(0, token_symbol);

  uint64_t token_amount = token_for_usd(usd_quantity, token_asset).amount;

  print(" token amount: "+std::to_string(token_amount));

  asset token_quantity = asset(token_amount, token_symbol);
  
  print(" token quantity: "+token_quantity.to_string());

  auto sitr = dailystats.find(buyer.value);
  if (sitr != dailystats.end()) {
    tokens_purchased = sitr->tokens_purchased;
  }
  
  uint64_t price_volatility_leeway = seeds_limit.amount / 20; // 5% leeway

  check( (seeds_limit.amount + price_volatility_leeway) >= tokens_purchased + token_amount, 
   "account: " + buyer.to_string() + 
   " symbol: " + paymentSymbol + 
   " tx_id: " + memo + 
   " usd_quantity: " + usd_quantity.to_string() + 
   " purchase limit overdrawn, tried to buy " + token_quantity.to_string() + 
   " limit: " + seeds_limit.to_string() + 
   " new total would be: " + std::to_string( (tokens_purchased + token_amount) / asset_factor_d(token_asset)));

  if (sitr == dailystats.end()) {
    dailystats.emplace(get_self(), [&](auto& s) {
      s.buyer_account = buyer;
      s.tokens_purchased = token_amount;
    });
  } else {
    dailystats.modify(sitr, get_self(), [&](auto& s) {
      s.tokens_purchased += token_amount;
    }); 
  }
  
  soldtable stb = sold.get_or_create(get_self(), soldtable());
  stb.total_sold = stb.total_sold + token_amount;
  sold.set(stb, get_self());

  update_price();

  action(
    permission_level{get_self(), "active"_n},
    hypha_contract, "transfer"_n,
    make_tuple(get_self(), buyer, token_quantity, memo)
  ).send();    
}

void sale::ontransfer(name buyer, name contract, asset tlos_quantity, string memo) {
  if (
    get_first_receiver() == contracts::tlostoken  &&    // from eosio token account
    contract == get_self() &&                           // received
    tlos_quantity.symbol == tlos_symbol                 // TLOS symbol
  ) {

    check(false, "TLOS sale disabled - need to implement use of oracle for price");

    check(!is_set(tlos_paused_flag), "TLOS purchase is paused.");

    configtable c = config.get();
  
    asset tlos_per_usd = c.tlos_per_usd;

    double tlos_q_double = tlos_quantity.amount / 10000.0;
    double tlos_per_usd_double = tlos_per_usd.amount / 10000.0;

    uint64_t usd_amount = (tlos_q_double * tlos_per_usd_double) * 10000;

    asset usd_asset = asset(usd_amount, usd_symbol);

    purchase_usd(buyer, usd_asset, "TLOS", memo);

    auto now = eosio::current_time_point().sec_since_epoch();

    string paymentId = buyer.to_string() + ": "+tlos_quantity.to_string() + " time: " + std::to_string(now);

    payhistory.emplace(_self, [&](auto& item) {
      item.id = payhistory.available_primary_key();
      item.recipientAccount = buyer;
      item.paymentSymbol = "TLOS";
      item.paymentQuantity = tlos_quantity.to_string();
      item.paymentId = paymentId;
      item.multipliedUsdValue = usd_asset.amount;
    });

  }
}

void sale::onhusd(name from, name to, asset quantity, string memo) {
  if (
    get_first_receiver() == husd_contract  &&
    to == get_self() &&                          
    quantity.symbol == husd_symbol                 
  ) {
    on_husd(from, to, quantity, memo);
  }
}

// void sale::testhusd(name from, name to, asset quantity) {
//   require_auth(get_self());
//   on_husd(from, to, quantity, "test_test");
// }

void sale::on_husd(name from, name to, asset quantity, string memo) {
    uint64_t usd_amount = quantity.amount * 100;

    check(false, "HUSD sale disabled");

    check(quantity.symbol == husd_symbol, "wrong symbol");

    asset usd_asset = asset(usd_amount, usd_symbol);

    purchase_usd(from, usd_asset, "HUSD", memo);

    auto now = eosio::current_time_point().sec_since_epoch();

    string paymentId = from.to_string() + ": "+quantity.to_string() + " time: " + std::to_string(now);

    payhistory.emplace(_self, [&](auto& item) {
      item.id = payhistory.available_primary_key();
      item.recipientAccount = from;
      item.paymentSymbol = "HUSD";
      item.paymentQuantity = quantity.to_string();
      item.paymentId = paymentId;
      item.multipliedUsdValue = usd_asset.amount;
    });

    string burn_memo = "burn";

    action(
      permission_level{get_self(), "active"_n},
      husd_contract, "transfer"_n,
      make_tuple(get_self(), "bank.hypha"_n, quantity, burn_memo)
    ).send();    

}

void sale::newpayment(name recipientAccount, string paymentSymbol, string paymentQuantity, string paymentId, uint64_t multipliedUsdValue) {

    require_auth(get_self());
 
    asset usd_asset = asset(multipliedUsdValue, usd_symbol);

    auto history_by_payment_id = payhistory.get_index<"bypaymentid"_n>();

    uint64_t key = std::hash<std::string>{}(paymentId);

    check( history_by_payment_id.find(key) == history_by_payment_id.end(), "duplicate transaction: "+paymentId);

    string memo = (paymentSymbol + ": " + paymentId).substr(0, 255);

    print("buying with USD: " + usd_asset.to_string() + " ");

    purchase_usd(recipientAccount, usd_asset, paymentSymbol, paymentId);

    payhistory.emplace(_self, [&](auto& item) {
      item.id = payhistory.available_primary_key();
      item.recipientAccount = recipientAccount;
      item.paymentSymbol = paymentSymbol;
      item.paymentQuantity = paymentQuantity;
      item.paymentId = paymentId;
      item.multipliedUsdValue = multipliedUsdValue;
    });

}

void sale::onperiod() {
  require_auth(get_self());
  
  auto sitr = dailystats.begin();
  
  while (sitr != dailystats.end()) {
    sitr = dailystats.erase(sitr);
  }


}

void sale::updatelimit(asset citizen_limit, asset resident_limit, asset visitor_limit) {
  require_auth(get_self());
  
  configtable c = config.get_or_create(get_self(), configtable());
  
  c.citizen_limit = citizen_limit;
  c.resident_limit = resident_limit;
  c.visitor_limit = visitor_limit;
  c.timestamp = current_time_point().sec_since_epoch();

  config.set(c, get_self());
}

// void sale::updateusd(asset token_per_usd) {
//   require_auth(get_self());

//   configtable c = config.get_or_create(get_self(), configtable());
  
//   c.token_per_usd = token_per_usd;
//   c.timestamp = current_time_point().sec_since_epoch();
  
//   config.set(c, get_self());
// }

void sale::updatetlos(asset tlos_per_usd) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.tlos_per_usd = tlos_per_usd;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}

void sale::update_price() {

  soldtable stb = sold.get_or_create(get_self(), soldtable());
  uint64_t total_sold = stb.total_sold;

  price_table p = price.get_or_create(get_self(), price_table());

  configtable c = config.get_or_create(get_self(), configtable());

  auto ritr = rounds.begin();

  while(true) {
    
    check(ritr != rounds.end(), "No more rounds - sold out");

    if (total_sold < ritr -> max_sold) {
      p.current_round_id = ritr -> id;
      p.current_token_per_usd = ritr -> token_per_usd;
      p.remaining = ritr->max_sold - total_sold;

      price.set(p, get_self());

      c.token_per_usd = ritr -> token_per_usd;
      c.timestamp = current_time_point().sec_since_epoch();

      config.set(c, get_self());

      break;
    }

    ritr++;
  }

  price_history_update();

}

ACTION sale::addround(uint64_t volume, asset token_per_usd) {
  require_auth(get_self());

  check(token_per_usd.amount > 0, "seeds per usd must be > 0");
  check(volume > 0, "volume must be > 0");

  check(token_per_usd.symbol.precision() == 2, "expected precision 2 for HYPHA sale");

  uint64_t prev_vol = 0;

  auto rounds_number = std::distance(rounds.begin(), rounds.end());

  auto previtr = rounds.find(rounds_number - 1);
  if (previtr != rounds.end()) {
    prev_vol = previtr -> max_sold;
  } else {
    check(rounds_number == 0, "invalid round id - must be continuous");
  }

  rounds.emplace(_self, [&](auto& item) {
    item.id = rounds_number;
    item.token_per_usd = token_per_usd;
    item.max_sold = prev_vol + volume; 
  });
}

ACTION sale::updatevol(uint64_t round_id, uint64_t volume) {
  require_auth(get_self());

  price_table p = price.get_or_create(get_self(), price_table());
  check(round_id > p.current_round_id, "cannot change volume on past or already started rounds, only on future rounds");

  uint64_t prev_vol = 0;

  auto previtr = rounds.find(round_id - 1);
  if (previtr != rounds.end()) {
    prev_vol = previtr -> max_sold;
  } else {
    check(round_id == 0, "invalid round id - must be continuous");
  }

  auto ritr = rounds.find(round_id);

  while(ritr != rounds.end()) {
    uint64_t max_sold = prev_vol + volume;
    rounds.modify(ritr, _self, [&](auto& item) {
        item.max_sold = max_sold;
    });
    prev_vol = max_sold;
    ritr++;
  }

}

ACTION sale::initsale() {
  require_auth(get_self());
  initrounds(
      uint64_t(100000) * uint64_t(100), // "100,000.00 HYPHA"
      asset(100, hypha_symbol), // 1.00 USD / HYPHA  ==> 1.00 HYPHA / USD  
      1.1, // 10% increase per round
      9 // 9 rounds
  );
}

ACTION sale::initrounds(uint64_t volume_per_round, asset initial_token_per_usd, double increment_factor, uint64_t num_rounds) {
  require_auth(get_self());

  check(initial_token_per_usd.symbol == hypha_symbol, "Only HYPHA allowed - example '1.00 HYPHA'");

  auto ritr = rounds.begin();
  while(ritr != rounds.end()){
    ritr = rounds.erase(ritr);
  }

  uint64_t token_per_usd = initial_token_per_usd.amount;
  double asset_factor =  asset_factor_d(initial_token_per_usd);


  double usd_per_token = 1.0 / (token_per_usd / asset_factor);

  for(int i=0; i<num_rounds; i++) {
    addround(volume_per_round, asset(token_per_usd, hypha_symbol));
    usd_per_token *= increment_factor;
    token_per_usd = uint64_t(round( asset_factor / usd_per_token));
  }

  update_price();

}

ACTION sale::incprice() {
  require_auth(get_self());

  double increment_factor = 1.1; // 10% increase

  price_table p = price.get();
  auto ritr = rounds.find(p.current_round_id);
  check(ritr != rounds.end(), "price table has invalid round id or sale has ended");

  print(std::to_string(p.current_round_id)+ " XX" );

  double asset_factor = asset_factor_d(asset(0, hypha_symbol));

  while(ritr != rounds.end()) {
    double usd_per_token = 1.0 / (ritr->token_per_usd.amount / asset_factor);
    usd_per_token *= increment_factor;
    uint64_t token_per_usd = uint64_t(round( asset_factor / usd_per_token));
    asset val = asset(token_per_usd, hypha_symbol);

    //print(std::to_string(ritr->id) + ": " + ritr->token_per_usd.to_string() + "---> " + val.to_string() + "\n ");
    rounds.modify(ritr, _self, [&](auto& item) {
      item.token_per_usd = val;
    });
    ritr++;
  }

  update_price();
}

ACTION sale::priceupdate() {
  require_auth(get_self());
  price_history_update();
}

void sale::price_history_update() {

  price_table p = price.get();

  auto phitr = pricehistory.rbegin();

  if (
    (phitr != pricehistory.rend() && p.current_token_per_usd != phitr -> seeds_usd) ||
    (phitr == pricehistory.rend())
  ) {
    pricehistory.emplace(_self, [&](auto & ph){
      ph.id = pricehistory.available_primary_key();
      ph.seeds_usd = p.current_token_per_usd;
      ph.date = eosio::current_time_point();
    });
  }
}

ACTION sale::setflag(name flagname, uint64_t value) {
  require_auth(get_self());

  auto fitr = flags.find(flagname.value);
  if (fitr == flags.end()) {
    flags.emplace(get_self(), [&](auto& item) {
      item.param = flagname;
      item.value = value;
    });
  } else {
    flags.modify(fitr, get_self(), [&](auto& item) {
      item.param = flagname;
      item.value = value;
    });
  } 
}

ACTION sale::pause() {
  require_auth(get_self());

  auto fitr = flags.find(paused_flag.value);
  if (fitr == flags.end()) {
    flags.emplace(get_self(), [&](auto& item) {
      item.param = paused_flag;
      item.value = 1;
    });
  } else {
    flags.modify(fitr, get_self(), [&](auto& item) {
      item.param = paused_flag;
      item.value = 1;
    });
  } 
}

ACTION sale::unpause() {
  require_auth(get_self());

  auto fitr = flags.find(paused_flag.value);
  if (fitr != flags.end()) {
    flags.modify(fitr, get_self(), [&](auto& item) {
      item.value = 0;
    });
  }
}

bool sale::is_paused() {
  auto fitr = flags.find(paused_flag.value);
  if (fitr != flags.end()) {
    return fitr->value > 0;
  }
  return false;
}

bool sale::is_set(name flag) {
  auto fitr = flags.find(flag.value);
  if (fitr != flags.end()) {
    return fitr->value > 0;
  }
  return false;
}
