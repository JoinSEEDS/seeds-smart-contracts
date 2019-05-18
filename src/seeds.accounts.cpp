#include <seeds.accounts.hpp>

void accounts::reset() {
  require_auth(_self);
  
  auto aitr = apps.begin();
  while (aitr != apps.end()) {
    aitr = apps.erase(aitr);
  }
  
  auto uitr = users.begin();
  while (uitr != users.end()) {
    uitr = users.erase(uitr);
  }
}

void accounts::adduser(name account)
{
  require_auth(_self);
  check(is_account(account), "no account");
  
  users.emplace(_self, [&](auto& user) {
      user.account = account;
  });
}

void accounts::addapp(name account)
{
  require_auth(_self);
  check(is_account(account), "no account");
  
  apps.emplace(_self, [&](auto& app) {
    app.account = account;
  });
}

void accounts::addrequest(name app, name user, string owner_key, string active_key)
{
  require_auth(app);

  check(is_account(user) == false, "existing user");
  check(requests.find(user.value) == requests.end(), "existing request");
  
  requests.emplace(_self, [&](auto& request) {
    request.app = app;
    request.user = user;
    request.owner_key = owner_key;
    request.active_key = active_key;
  });
}

void accounts::fulfill(name app, name user)
{
  require_auth(_self);

  auto ritr = requests.find(user.value);
  check(ritr != requests.end(), "no request");
  check(ritr->app == app, "another application");

  if (is_account(user) == false) {
    buyaccount(user, ritr->owner_key, ritr->active_key);
  }
  
  requests.erase(ritr);
}

void accounts::buyaccount(name account, string owner_key, string active_key)
{
  check(is_account(account) == false, "existing account");
  
  asset ram = asset(3000, network_symbol);
  asset cpu = asset(900, network_symbol);
  asset net = asset(100, network_symbol);
  
  authority owner_auth = keystring_authority(owner_key);
  authority active_auth = keystring_authority(active_key);
  
  action(
    permission_level{_self, "owner"_n},
    "eosio"_n, "newaccount"_n,
    std::make_tuple(_self, account, owner_auth, active_auth))
    .send();
  
  action(
    permission_level{_self, "owner"_n},
    "eosio"_n, "buyram"_n,
    std::make_tuple(_self, account, ram))
    .send();
  
  action(
    permission_level{_self, "owner"_n},
    "eosio"_n, "delegatebw"_n,
    std::make_tuple(_self, account, net, cpu, 1))
    .send();
}