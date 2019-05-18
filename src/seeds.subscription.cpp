#include <seeds.subscription.hpp>

void subscription::reset() {
  require_auth(_self);

  configure(name("tokenaccnt"), "token"_n.value);
  configure(name("bankaccnt"), "bank"_n.value);

  auto pitr = providers.begin();
  
  while (pitr != providers.end()) {
    name app = pitr->app;
    subscription_tables subs(_self, app.value);
    auto sitr = subs.begin();
    
    while(sitr != subs.end()) {
      sitr = subs.erase(sitr);
    }
    
    pitr = providers.erase(pitr);
  }
}

void subscription::configure(name param, uint64_t value) {
  require_auth(_self);

  auto citr = config.find(param.value);

  if (citr == config.end()) {
    config.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  } else {
    config.modify(citr, _self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  }
}

void subscription::create(name app, asset price)
{
  require_auth(app);
  check_asset(price);
  
  check(apps.find(app.value) != apps.end(), "no application");
  check(providers.find(app.value) == providers.end(), "existing provider");
  
  providers.emplace(app, [&](auto& provider) {
    provider.app = app;
    provider.price = price;
    provider.balance = asset(0, seeds_symbol);
    provider.active = true;
  });
}

void subscription::update(name app, bool active, asset price)
{
  require_auth(app);
  check_asset(price);
  
  auto pitr = providers.find(app.value);
  check(pitr != providers.end(), "no provider");

  providers.modify(pitr, app, [&](auto& provider) {
    provider.active = active;
    provider.price = price;
  });
}

void subscription::increase(name from, name to, asset quantity, string memo)
{

}

void subscription::enable(name user, name app)
{
  require_auth(user);
  
  auto pitr = providers.find(app.value);
  asset price = pitr->price;
  
  subscription_tables subs(_self, app.value);
  auto sitr = subs.find(user.value);
  check(sitr != subs.end(), "no subscription");
  
  subs.modify(sitr, user, [&](auto& sub) {
    if (sub.deposit > sub.invoice && sub.deposit > price) {
      sub.active = true;
    }
  });
}

void subscription::disable(name user, name app)
{
  require_auth(user);
  
  subscription_tables subs(_self, app.value);
  
  auto sitr = subs.find(user.value);
  check(sitr != subs.end(), "no subscription");
  
  subs.modify(sitr, user, [&](auto& sub) {
    sub.active = false;
  });
}

void subscription::onblock()
{
  require_auth(_self);
  
  auto pitr = providers.begin();
  
  while (pitr != providers.end()) {
    if (pitr->active == true) {
      name app = pitr->app;
      asset price = pitr->price;
  
      asset payout = asset(0, seeds_symbol);
      
      subscription_tables subs(_self, app.value);
      auto sitr = subs.begin();
      
      while(sitr != subs.end()) {
        if (sitr->active == true) {
          subs.modify(sitr, _self, [&](auto& sub) {
            sub.invoice += price;
            if (sub.invoice > sub.deposit) {
              sub.active = false;
            }
          });
          
          payout += price;
        }
        sitr++;
      }
      
      if (payout > asset(0, seeds_symbol)) {
        providers.modify(pitr, _self, [&](auto& provider) {
          provider.balance += payout;
        });
      }
    }
    pitr++;
  }
}

void subscription::claimpayout(name app)
{
  require_auth(app);
  
  auto pitr = providers.find(app.value);
  check(pitr != providers.end(), "no provider");
  
  auto payout = pitr->balance;
  
  if (payout > asset(0, seeds_symbol)) {
    providers.modify(pitr, app, [&](auto& provider) {
      provider.balance = asset(0, seeds_symbol);
    });
    
    withdraw(app, payout);
  }
}

void subscription::deposit(asset quantity)
{
  check_asset(quantity);
  
  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;
  
  token::transfer_action action{name(token_account), {_self, "active"_n}};
  action.send(_self, name(bank_account), quantity, "");
}

void subscription::withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;
  
  token::transfer_action action{name(token_account), {name(bank_account), "active"_n}};
  action.send(name(bank_account), beneficiary, quantity, "");
}

void subscription::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

void subscription::check_asset(asset quantity)
{
  check(quantity.is_valid(), "invalid asset");
  check(quantity.symbol == seeds_symbol, "invalid asset");
}
