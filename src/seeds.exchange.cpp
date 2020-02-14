#include <seeds.exchange.hpp>

void exchange::reset() {
  require_auth(_self);

  config.remove();

  configtable c = config.get_or_create(get_self(), configtable());

  // TLOS 0.05368
  // SEEDS 0.01
  // Seeds per TLOS = 5.3611 * 10000 = 5.36 * 10000

  // c.rate = asset(53600, seeds_symbol);                      // 5.63
  c.visitor_limit =   asset(25000 * 10000, seeds_symbol);        // USD 250 / wk = 25,000 SEEDS
  c.resident_limit =  asset( uint64_t(250000) * uint64_t(10000), seeds_symbol);        // 250,000 
  c.citizen_limit =   asset( uint64_t(250000) * uint64_t(10000), seeds_symbol);       // 250,000 
  c.timestamp = current_time_point().sec_since_epoch();

  config.set(c, get_self());

}

void exchange::purchase(name buyer, name contract, asset tlos_quantity, string memo) {
  if (contract == get_self()) {

    eosio::multi_index<"users"_n, tables::user_table> users(contracts::accounts, contracts::accounts.value);

    auto uitr = users.find(buyer.value);
    check(uitr != users.end(), "not a seeds user " + buyer.to_string());

    configtable c = config.get();
    asset seeds_per_tlos = c.rate;
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
    
    asset tlos_as_seeds = asset(tlos_quantity.amount, seeds_symbol);
    uint64_t seeds_amount = (tlos_quantity.amount * seeds_per_tlos.amount) / 10000;
    asset seeds_quantity = asset(seeds_amount, seeds_symbol);
    
    auto sitr = dailystats.find(buyer.value);
    if (sitr != dailystats.end()) {
      seeds_purchased = sitr->seeds_purchased;
    }
  
    check(tlos_quantity.symbol == tlos_symbol, "invalid asset, expected tlos token");
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
    
    soldtable s = sold.get_or_create(get_self(), soldtable());
    s.total_sold += seeds_quantity.amount;

    action(
      permission_level{get_self(), "active"_n},
      contracts::token, "transfer"_n,
      make_tuple(get_self(), buyer, seeds_quantity, string(""))
    ).send();    
  }
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

void exchange::updaterate(asset seeds_per_tlos) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.rate = seeds_per_tlos;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}
