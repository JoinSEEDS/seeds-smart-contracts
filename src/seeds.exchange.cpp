#include <seeds.exchange.hpp>

void exchange::purchase(name buyer, name contract, asset tlos_quantity, string memo) {
  if (contract == get_self()) {
    configtable c = config.get();
    uint64_t seeds_per_tlos = c.rate;
    uint64_t seeds_per_day = c.limit * 10000;
    uint64_t seeds_purchased = 0;
    
    uint64_t seeds_amount = tlos_quantity.amount * seeds_per_tlos;
    asset seeds_quantity = asset(seeds_amount, seeds_symbol);
    
    auto sitr = dailystats.find(buyer.value);
    if (sitr != dailystats.end()) {
      seeds_purchased = sitr->seeds_purchased;
    }
  
    check(tlos_quantity.symbol == tlos_symbol, "invalid asset, expected tlos token");
    check(seeds_per_day >= seeds_purchased + seeds_amount, "purchase limit overdrawn, tried to buy " + seeds_quantity.to_string() + " .. but available only " + std::to_string(seeds_per_day));

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
    
    action(
      permission_level{get_self(), "active"_n},
      contracts::token, "transfer"_n,
      make_tuple(get_self(), buyer, seeds_quantity, string(""))
    ).send();    
  }
}

void exchange::dailyreset() {
  require_auth(get_self());
  
  auto sitr = dailystats.begin();
  
  while (sitr != dailystats.end()) {
    sitr = dailystats.erase(sitr);
  }
}

void exchange::updatelimit(uint64_t seeds_per_day) {
  require_auth(get_self());
  
  configtable c = config.get_or_create(get_self(), configtable());
  
  c.limit = seeds_per_day;
  
  config.set(c, get_self());
}

void exchange::updaterate(uint64_t seeds_per_tlos) {
  require_auth(get_self());

  configtable c = config.get_or_create(get_self(), configtable());
  
  c.rate = seeds_per_tlos;
  c.timestamp = current_time_point().sec_since_epoch();
  
  config.set(c, get_self());
}
