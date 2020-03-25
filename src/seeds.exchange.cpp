#include <seeds.exchange.hpp>
#include <cmath>

void exchange::reset() {
  require_auth(_self);

  config.remove();

  auto pitr = payhistory.begin();
  while(pitr != payhistory.end()) {
    pitr = payhistory.erase(pitr);
  }

  asset citizen_limit =  asset(uint64_t(2500000000), seeds_symbol);
  asset resident_limit =  asset(uint64_t(2500000000), seeds_symbol);
  asset visitor_limit =  asset(25000 * 10000, seeds_symbol);

  asset seeds_per_usd =  asset( uint64_t(1000000), seeds_symbol);
  asset tlos_per_usd =  asset(0.03 * 10000, seeds_symbol);

  updatelimit(citizen_limit, resident_limit, visitor_limit);
  updateusd(seeds_per_usd);
  updatetlos(tlos_per_usd);

}

void exchange::purchase_usd(name buyer, asset usd_quantity, string memo) {

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
  
  uint64_t seeds_amount = (usd_quantity.amount * seeds_per_usd.amount) / 10000;
  asset seeds_quantity = asset(seeds_amount, seeds_symbol);
  
  auto sitr = dailystats.find(buyer.value);
  if (sitr != dailystats.end()) {
    seeds_purchased = sitr->seeds_purchased;
  }

  check(seeds_limit.amount >= seeds_purchased + seeds_amount, "purchase limit overdrawn, tried to buy " + seeds_quantity.to_string() + " limit: " + seeds_limit.to_string() + " new total would be: " + std::to_string(seeds_purchased + seeds_amount));

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

  action(
    permission_level{get_self(), "active"_n},
    contracts::token, "transfer"_n,
    make_tuple(get_self(), buyer, seeds_quantity, memo)
  ).send();    
}

void exchange::buytlos(name buyer, name contract, asset tlos_quantity, string memo) {
  if (contract == get_self()) {

    check(tlos_quantity.symbol == tlos_symbol, "invalid asset, expected tlos token");

    configtable c = config.get();
  
    asset tlos_per_usd = c.tlos_per_usd;

    uint64_t usd_amount = (tlos_quantity.amount * tlos_per_usd.amount) / 10000;

    asset usd_asset = asset(usd_amount, usd_symbol);

    purchase_usd(buyer, usd_asset, memo);
  }
}


void exchange::newpayment(name recipientAccount, string paymentSymbol, string paymentId, uint64_t multipliedUsdValue) {

    require_auth(get_self());

    asset usd_asset = asset(multipliedUsdValue, usd_symbol);

    auto history_by_payment_id = payhistory.get_index<"bypaymentid"_n>();

    uint64_t key = std::hash<std::string>{}(paymentId);

    check( history_by_payment_id.find(key) == history_by_payment_id.end(), "duplicate transaction: "+paymentId);

    string memo = (paymentSymbol + ": " + paymentId).substr(0, 255);

    purchase_usd(recipientAccount, usd_asset, paymentId);

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

void exchange::updateusd(asset seeds_per_usd) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.seeds_per_usd = seeds_per_usd;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}

void exchange::updatetlos(asset tlos_per_usd) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.tlos_per_usd = tlos_per_usd;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}

ACTION exchange::updateprice() {
  //require_auth(get_self());

  soldtable stb = sold.get_or_create(get_self(), soldtable());
  uint64_t total_sold = stb.total_sold;

  price_table p = price.get_or_create(get_self(), price_table());
  
  auto ritr = rounds.begin();

  while(true) {
    
    check(ritr != rounds.end(), "No more rounds - sold out");

    if (total_sold < ritr -> max_sold) {
      p.current_round_id = ritr -> id;
      p.current_seeds_per_usd = ritr -> seeds_per_usd;
      p.remaining = ritr->max_sold - total_sold;

      price.set(p, get_self());
      break;
    }

    ritr++;
  }
}

ACTION exchange::addround(uint64_t volume, asset seeds_per_usd) {
  require_auth(get_self());

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
  double increment_factor = 1.033; // 3.3% per round = factor of 1.033 per round

  for(int i=0; i<50; i++) {
    addround(volume_per_round, asset(seeds_per_usd, seeds_symbol));
    double usd_per_seeds = 1.0 / seeds_per_usd;
    usd_per_seeds *= increment_factor;
    seeds_per_usd = uint64_t(round( 1.0 / usd_per_seeds));
  }
}
