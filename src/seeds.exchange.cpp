#include <seeds.exchange.hpp>
#include <cmath>

void exchange::reset() {
  require_auth(_self);

  config.remove();

  asset citizen_limit =  asset(uint64_t(2600000000), seeds_symbol);
  asset resident_limit =  asset(uint64_t(2600000000), seeds_symbol);
  asset visitor_limit =  asset(26000 * 10000, seeds_symbol);

  asset tlos_per_usd =  asset(0.03 * 10000, seeds_symbol);

  updatelimit(citizen_limit, resident_limit, visitor_limit);
  updatetlos(tlos_per_usd);

  unpause();
  setflag(tlos_paused_flag, 1);

  // COMMENT in for testing, never check in commented in
/**
  // we never want to erase rounds or sold table or history except for unit testing
  
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
/**/

}

asset exchange::seeds_for_usd(asset usd_quantity) {
  update_price();

  soldtable s = sold.get_or_create(get_self(), soldtable());
 
  double usd_total = double(usd_quantity.amount);
  double usd_remaining = usd_total;
  double seeds_amount = 0.0;

  auto ritr = rounds.begin(); //rounds.find(p -> current_round);

  uint64_t round_start_volume = 0;

  while(ritr != rounds.end() && usd_remaining > 0) {
    uint64_t round_end_volume = ritr->max_sold;

    if (s.total_sold < round_end_volume) {
      double usd_per_seeds = 10000.0 / double(ritr->seeds_per_usd.amount);

      // num available
      double available_in_round = round_end_volume - std::max(round_start_volume, s.total_sold);

      // price of available seeds
      double usd_available = available_in_round * usd_per_seeds;

      // if < usd amount remaining -> calculate, add, and exit
      if (usd_available >= usd_remaining) {
        seeds_amount += (usd_remaining * ritr->seeds_per_usd.amount) / 10000;
        usd_remaining = 0;
        break;
      } else {
        usd_remaining -= usd_available;
        seeds_amount += available_in_round;
      }
    }
    
    round_start_volume = round_end_volume;
    ritr++;

    check(ritr != rounds.end(), "not enough funds available. requested USD value: "+std::to_string(usd_total/10000.0) + 
    " available USD value: "+std::to_string( (usd_total - usd_remaining) / 10000.0) + " max vol: " + std::to_string(round_end_volume));

  }

  return asset(seeds_amount, seeds_symbol);
}

void exchange::purchase_usd(name buyer, asset usd_quantity, string paymentSymbol, string memo) {
  check(!is_paused(), "Contract is paused - no purchase possible.");

  eosio::multi_index<"users"_n, tables::user_table> users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(buyer.value);
  check(uitr != users.end(), "not a seeds user " + buyer.to_string());

  configtable c = config.get();

  asset seeds_per_usd = c.seeds_per_usd;
  uint64_t seeds_purchased = 0;

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
  
  uint64_t seeds_amount = seeds_for_usd(usd_quantity).amount;
  asset seeds_quantity = asset(seeds_amount, seeds_symbol);
  
  auto sitr = dailystats.find(buyer.value);
  if (sitr != dailystats.end()) {
    seeds_purchased = sitr->seeds_purchased;
  }

  check(seeds_limit.amount >= seeds_purchased + seeds_amount, 
   "account: " + buyer.to_string() + 
   " symbol: " + paymentSymbol + 
   " tx_id: " + memo + 
   " usd_quantity: " + usd_quantity.to_string() + 
   " purchase limit overdrawn, tried to buy " + seeds_quantity.to_string() + 
   " limit: " + seeds_limit.to_string() + 
   " new total would be: " + std::to_string( (seeds_purchased + seeds_amount) / 10000.0));

  if (sitr == dailystats.end()) {
    dailystats.emplace(get_self(), [&](auto& s) {
      s.buyer_account = buyer;
      s.seeds_purchased = seeds_amount;
    });
  } else {
    dailystats.modify(sitr, get_self(), [&](auto& s) {
      s.seeds_purchased += seeds_amount;
    }); 
  }
  
  soldtable stb = sold.get_or_create(get_self(), soldtable());
  stb.total_sold = stb.total_sold + seeds_amount;
  sold.set(stb, get_self());

  update_price();

  action(
    permission_level{get_self(), "active"_n},
    contracts::token, "transfer"_n,
    make_tuple(get_self(), buyer, seeds_quantity, memo)
  ).send();    
}

void exchange::ontransfer(name buyer, name contract, asset tlos_quantity, string memo) {
  if (
    get_first_receiver() == contracts::tlostoken  &&    // from eosio token account
    contract == get_self() &&                           // received
    tlos_quantity.symbol == tlos_symbol                 // TLOS symbol
  ) {

    check(!is_set(tlos_paused_flag), "TLOS purchase is paused.");

    configtable c = config.get();
  
    asset tlos_per_usd = c.tlos_per_usd;

    uint64_t usd_amount = (tlos_quantity.amount * tlos_per_usd.amount) / 10000;

    asset usd_asset = asset(usd_amount, usd_symbol);

    purchase_usd(buyer, usd_asset, "TLOS", memo);
  }
}


void exchange::newpayment(name recipientAccount, string paymentSymbol, string paymentId, uint64_t multipliedUsdValue) {

    require_auth(get_self());

    asset usd_asset = asset(multipliedUsdValue, usd_symbol);

    auto history_by_payment_id = payhistory.get_index<"bypaymentid"_n>();

    uint64_t key = std::hash<std::string>{}(paymentId);

    check( history_by_payment_id.find(key) == history_by_payment_id.end(), "duplicate transaction: "+paymentId);

    string memo = (paymentSymbol + ": " + paymentId).substr(0, 255);

    purchase_usd(recipientAccount, usd_asset, paymentSymbol, paymentId);

    payhistory.emplace(_self, [&](auto& item) {
      item.id = payhistory.available_primary_key();
      item.recipientAccount = recipientAccount;
      item.paymentSymbol = paymentSymbol;
      item.paymentId = paymentId;
      item.multipliedUsdValue = multipliedUsdValue;
    });

}

void exchange::onperiod() {
  require_auth(get_self());
  
  auto sitr = dailystats.begin();
  
  while (sitr != dailystats.end()) {
    sitr = dailystats.erase(sitr);
  }


}

void exchange::updatelimit(asset citizen_limit, asset resident_limit, asset visitor_limit) {
  require_auth(get_self());
  
  configtable c = config.get_or_create(get_self(), configtable());
  
  c.citizen_limit = citizen_limit;
  c.resident_limit = resident_limit;
  c.visitor_limit = visitor_limit;
  c.timestamp = current_time_point().sec_since_epoch();

  config.set(c, get_self());
}

// void exchange::updateusd(asset seeds_per_usd) {
//   require_auth(get_self());

//   configtable c = config.get_or_create(get_self(), configtable());
  
//   c.seeds_per_usd = seeds_per_usd;
//   c.timestamp = current_time_point().sec_since_epoch();
  
//   config.set(c, get_self());
// }

void exchange::updatetlos(asset tlos_per_usd) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.tlos_per_usd = tlos_per_usd;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}

void exchange::update_price() {

  soldtable stb = sold.get_or_create(get_self(), soldtable());
  uint64_t total_sold = stb.total_sold;

  price_table p = price.get_or_create(get_self(), price_table());

  configtable c = config.get_or_create(get_self(), configtable());

  auto ritr = rounds.begin();

  while(true) {
    
    check(ritr != rounds.end(), "No more rounds - sold out");

    if (total_sold < ritr -> max_sold) {
      p.current_round_id = ritr -> id;
      p.current_seeds_per_usd = ritr -> seeds_per_usd;
      p.remaining = ritr->max_sold - total_sold;

      price.set(p, get_self());

      c.seeds_per_usd = ritr -> seeds_per_usd;
      c.timestamp = current_time_point().sec_since_epoch();

      config.set(c, get_self());

      break;
    }

    ritr++;
  }

  price_history_update();

}

ACTION exchange::addround(uint64_t volume, asset seeds_per_usd) {
  require_auth(get_self());

  check(seeds_per_usd.amount > 0, "seeds per usd must be > 0");
  check(volume > 0, "volume must be > 0");

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
    item.seeds_per_usd = seeds_per_usd;
    item.max_sold = prev_vol + volume; 
  });
}

ACTION exchange::initsale() {
  require_auth(get_self());
  initrounds(
      uint64_t(1100000) * uint64_t(10000), // "1,100,000.0000 SEEDS"
      asset(909091, seeds_symbol) // 0.011 USD / SEEDS  ==> 90.9090909091 SEEDS / USD  
  );
}

ACTION exchange::initrounds(uint64_t volume_per_round, asset initial_seeds_per_usd) {
  require_auth(get_self());

  auto ritr = rounds.begin();
  while(ritr != rounds.end()){
    ritr = rounds.erase(ritr);
  }

  uint64_t seeds_per_usd = initial_seeds_per_usd.amount;
  
  double usd_per_seeds = 1.0 / (seeds_per_usd / 10000.0);
  double increment_factor = 1.033; // 3.3% per round = factor of 1.033 per round

  for(int i=0; i<50; i++) {
    addround(volume_per_round, asset(seeds_per_usd, seeds_symbol));
    usd_per_seeds *= increment_factor;
    seeds_per_usd = uint64_t(round( 10000.0 / usd_per_seeds));
  }

  update_price();

}

ACTION exchange::incprice() {
  require_auth(get_self());

  double increment_factor = 1.033; // 3.3% increase

  price_table p = price.get();
  auto ritr = rounds.find(p.current_round_id);
  check(ritr != rounds.end(), "price table has invalid round id or sale has ended");

  print(std::to_string(p.current_round_id)+ " XX" );

  while(ritr != rounds.end()) {
    double usd_per_seeds = 1.0 / (ritr->seeds_per_usd.amount / 10000.0);
    usd_per_seeds *= increment_factor;
    uint64_t seeds_per_usd = uint64_t(round( 10000.0 / usd_per_seeds));
    asset val = asset(seeds_per_usd, seeds_symbol);

    //print(std::to_string(ritr->id) + ": " + ritr->seeds_per_usd.to_string() + "---> " + val.to_string() + "\n ");
    rounds.modify(ritr, _self, [&](auto& item) {
      item.seeds_per_usd = val;
    });
    ritr++;
  }

  update_price();
}

ACTION exchange::priceupdate() {
  require_auth(get_self());
  price_history_update();
}

void exchange::price_history_update() {

  price_table p = price.get();

  auto phitr = pricehistory.rbegin();

  if (
    (phitr != pricehistory.rend() && p.current_seeds_per_usd != phitr -> seeds_usd) ||
    (phitr == pricehistory.rend())
  ) {
    pricehistory.emplace(_self, [&](auto & ph){
      ph.id = pricehistory.available_primary_key();
      ph.seeds_usd = p.current_seeds_per_usd;
      ph.date = eosio::current_time_point();
    });
  }
}

ACTION exchange::setflag(name flagname, uint64_t value) {
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

ACTION exchange::pause() {
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

ACTION exchange::unpause() {
  require_auth(get_self());

  auto fitr = flags.find(paused_flag.value);
  if (fitr != flags.end()) {
    flags.modify(fitr, get_self(), [&](auto& item) {
      item.value = 0;
    });
  }
}

bool exchange::is_paused() {
  auto fitr = flags.find(paused_flag.value);
  if (fitr != flags.end()) {
    return fitr->value > 0;
  }
  return false;
}

bool exchange::is_set(name flag) {
  auto fitr = flags.find(flag.value);
  if (fitr != flags.end()) {
    return fitr->value > 0;
  }
  return false;
}

ACTION exchange::migrate() {
  require_auth(get_self());

  name old = "tlosto.seeds"_n;

  // typedef eosio::multi_index<"flags"_n, flags_table> flags_tables; 
  flags_tables other_flags(old, old.value);
  auto fitr = other_flags.begin();
  while(fitr != other_flags.end()) {
    flags.emplace(get_self(), [&](auto& item) {
      item.param = fitr->param;
      item.value = fitr->value;
    }); 
    fitr++;
  }

  // typedef singleton<"config"_n, configtable> configtables;
  configtables other_config(old, old.value);
  configtable c_other = other_config.get();
  configtable c = config.get_or_create(get_self(), configtable());
  c.seeds_per_usd = c_other.seeds_per_usd;
  c.tlos_per_usd = c_other.tlos_per_usd;
  c.citizen_limit = c_other.citizen_limit;
  c.resident_limit = c_other.resident_limit;
  c.visitor_limit = c_other.visitor_limit;
  c.timestamp = c_other.timestamp;
  config.set(c, get_self());

  // typedef singleton<"sold"_n, soldtable> soldtables;
  soldtables other_sold(old, old.value);
  soldtable s_other = other_sold.get();
  soldtable s = sold.get_or_create(get_self(), soldtable());
  s.id = s_other.id;
  s.total_sold = s_other.total_sold;
  sold.set(s, get_self());

  // typedef singleton<"price"_n, price_table> price_tables;
  price_tables other_price(old, old.value);
  price_table p_other = other_price.get();
  price_table p = price.get_or_create(get_self(), price_table());
  p.id = p_other.id;
  p.current_round_id = p_other.current_round_id;
  p.current_seeds_per_usd = p_other.current_seeds_per_usd;
  p.remaining = p_other.remaining;
  price.set(p, get_self());

  // typedef eosio::multi_index<"pricehistory"_n, price_history_table> price_history_tables;
  price_history_tables other_price_history(old, old.value);
  auto phiter = other_price_history.begin();
  while (phiter != other_price_history.end()) {
    pricehistory.emplace(get_self(), [&](auto& item) {
      item.id = phiter->id;
      item.seeds_usd = phiter->seeds_usd;
      item.date = phiter->date;
    });
    phiter++;
  }

  // typedef multi_index<"dailystats"_n, stattable> stattables;
  stattables other_dailystats(old, old.value);
  auto siter = other_dailystats.begin();
  while (siter != other_dailystats.end()) {
    dailystats.emplace(get_self(), [&](auto& item) {
      item.buyer_account = siter->buyer_account;
      item.seeds_purchased = siter->seeds_purchased;
    });
    siter++;
  }

  // typedef multi_index<"rounds"_n, round_table> round_tables;
  round_tables other_rounds(old, old.value);
  auto riter = other_rounds.begin();
  while (riter != other_rounds.end()) {
    rounds.emplace(get_self(), [&](auto& item) {
      item.id = riter->id;
      item.max_sold = riter->max_sold;
      item.seeds_per_usd = riter->seeds_per_usd;
    });
    riter++;
  }

  // typedef eosio::multi_index<"payhistory"_n, payhistory_table,
  //   indexed_by<"bypaymentid"_n,const_mem_fun<payhistory_table, uint64_t, &payhistory_table::by_payment_id>>
  // > payhistory_tables;
  payhistory_tables other_pay_history(old, old.value);
  auto piter = other_pay_history.begin();
  while (piter != other_pay_history.end()) {
    payhistory.emplace(get_self(), [&](auto& item) {
      item.id = piter->id;
      item.recipientAccount = piter->recipientAccount;
      item.paymentSymbol = piter->paymentSymbol;
      item.paymentId = piter->paymentId;
      item.multipliedUsdValue = piter->multipliedUsdValue;
    });
    piter++;
  }


  
}