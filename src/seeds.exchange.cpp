#include <seeds.exchange.hpp>

void exchange::reset() {
  require_auth(_self);

  config.remove();

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
  
  asset tlos_as_seeds = asset(usd_quantity.amount, seeds_symbol);
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
    make_tuple(get_self(), buyer, seeds_quantity, string(""))
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
