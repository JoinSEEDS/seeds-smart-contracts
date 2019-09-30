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

  auto ritr = requests.begin();
  while (ritr != requests.end()) {
    ritr = requests.erase(ritr);
  }
  
  auto refitr = refs.begin();
  while (refitr != refs.end()) {
    refitr = refs.erase(refitr);
  }
}

void accounts::migrate(name account,
        name status,
        name type,
        string nickname,
        string image,
        string story,
        string roles,
        string skills,
        string interests,
        uint64_t reputation
)
{
    require_auth(_self);

  users.emplace(_self, [&](auto& user) {
    user.account = account;
    user.status = status;
    user.type = type;
    user.nickname = nickname;
    user.image = image;
    user.story = story;
    user.roles = roles;
    user.skills = skills;
    user.interests = interests;
    user.reputation = reputation;
    user.timestamp = now();
  });
}

void accounts::joinuser(name account)
{
  require_auth(_self);

  auto uitr = users.find(account.value);
  check(uitr == users.end(), "existing user");

  users.emplace(_self, [&](auto& user) {
    user.account = account;
    user.status = name("visitor");
    user.reputation = 0;
    user.type = name("individual");
    user.timestamp = now();
  });
}


void accounts::adduser(name account, string nickname)
{
  require_auth(_self);
  check(is_account(account), "no account");

  auto uitr = users.find(account.value);
  check(uitr == users.end(), "existing user");

  users.emplace(_self, [&](auto& user) {
      user.account = account;
      user.status = name("visitor");
      user.reputation = 0;
      user.type = name("individual");
      user.nickname = nickname;
      user.timestamp = now();
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

void accounts::addref(name referrer, name invited)
{
    require_auth(_self);
    check(is_account(referrer), "wrong referral");
    check(is_account(invited), "wrong invited");

    refs.emplace(_self, [&](auto& ref) {
      ref.referrer = referrer;
      ref.invited = invited;
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

  users.emplace(_self, [&](auto& item) {
      item.account = user;
      item.status = name("visitor");
      item.reputation = 0;
  });
}

void accounts::update(name user, name type, string nickname, string image, string story, string roles, string skills, string interests)
{
    require_auth(user);

    check(type == name("individual") || type == name("organization"), "invalid type");

    auto uitr = users.find(user.value);
    users.modify(uitr, _self, [&](auto& user) {
      user.type = type;
      user.nickname = nickname;
      user.image = image;
      user.story = story;
      user.roles = roles;
      user.skills = skills;
      user.interests = interests;
    });
}

void accounts::makeresident(name user)
{
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("visitor"), "user is not a visitor");

    auto bitr = balances.find(user.value);

    transaction_tables transactions(name("seedstoken12"), seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);

    uint64_t invited_users_number = std::distance(refs.lower_bound(user.value), refs.upper_bound(user.value));
/*
    while (ritr != invited_users.end() && ritr->referrer == user) {
      invited_users_number++;
      ritr++;
    }
*/

    check(bitr->planted.amount >= 50, "user has less than required seeds planted");
    check(titr->transactions_number >= 1, "user has less than required transactions number");
    check(invited_users_number >= 1, "user has less than required referrals");
    check(uitr->reputation >= 100, "user has less than required reputation");

    users.modify(uitr, _self, [&](auto& user) {
        user.status = name("resident");
    });
}

void accounts::makecitizen(name user)
{
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("resident"), "user is not a resident");

    auto bitr = balances.find(user.value);

    transaction_tables transactions(name("seedstoken12"), seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);

    uint64_t invited_users_number = std::distance(refs.lower_bound(user.value), refs.upper_bound(user.value));

    check(bitr->planted.amount >= 100, "user has less than required seeds planted");
    check(titr->transactions_number >= 2, "user has less than required transactions number");
    check(invited_users_number >= 3, "user has less than required referrals");
    check(uitr->reputation >= 100, "user has less than required reputation");

    users.modify(uitr, _self, [&](auto& user) {
        user.status = name("citizen");
    });
}

void accounts::buyaccount(name account, string owner_key, string active_key)
{
  check(is_account(account) == false, "existing account");

  asset ram = asset(28000, network_symbol);
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
