#include <seeds.accounts.hpp>
#include <eosio/system.hpp>

void accounts::reset() {
  require_auth(_self);

  auto uitr = users.begin();
  while (uitr != users.end()) {
    vouch_tables vouch(get_self(), uitr->account.value);
    auto vitr = vouch.begin();
    while (vitr != vouch.end()) {
      vitr = vouch.erase(vitr);
    }

    uitr = users.erase(uitr);
  }

  auto refitr = refs.begin();
  while (refitr != refs.end()) {
    refitr = refs.erase(refitr);
  }

  auto repitr = reps.begin();
  while (repitr != reps.end()) {
    repitr = reps.erase(repitr);
  }
}

void accounts::migrateall()
{
  name accounts_old = name("seedsaccntsx");
  
  user_tables users_old(accounts_old, accounts_old.value);
  
  auto uitr = users_old.begin();
  
  while (uitr != users_old.end()) {
    auto olduser = *uitr;
    
    migrate(
      olduser.account, 
      olduser.status, 
      olduser.type, 
      olduser.nickname,
      olduser.image,
      olduser.story,
      olduser.roles,
      olduser.skills,
      olduser.interests,
      olduser.reputation,
      olduser.timestamp
    );
    
    uitr++;
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
        uint64_t reputation,
        uint64_t timestamp
)
{
  require_auth(_self);

  user_tables users(contracts::accounts, contracts::accounts.value);

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
    user.timestamp = timestamp;
  });
}

void accounts::joinuser(name account)
{
  require_auth(account);

  auto uitr = users.find(account.value);
  check(uitr == users.end(), "existing user");

  users.emplace(_self, [&](auto& user) {
    user.account = account;
    user.status = name("visitor");
    user.reputation = 0;
    user.type = name("individual");
    user.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void accounts::adduser(name account, string nickname)
{
  require_auth(get_self());
  check(is_account(account), "no account");

  auto uitr = users.find(account.value);
  check(uitr == users.end(), "existing user");

  users.emplace(_self, [&](auto& user) {
      user.account = account;
      user.status = name("visitor");
      user.reputation = 0;
      user.type = name("individual");
      user.nickname = nickname;
      user.timestamp = eosio::current_time_point().sec_since_epoch();
  });
}

void accounts::vouch(name sponsor, name account) {
  require_auth(sponsor);
  check_user(sponsor);
  check_user(account);

  vouch_tables vouch(get_self(), account.value);

  auto vitr = vouch.find(sponsor.value);

  check(vitr == vouch.end(), "already vouched");

  auto uitrs = users.find(sponsor.value);
  auto uitra = users.find(account.value);

  name sponsor_status = uitrs->status;
  name account_status = uitra->status;

  check(account_status == name("visitor"), "account should be visitor");
  check(sponsor_status == name("citizen") || sponsor_status == name("resident"), "sponsor should be a citizen");

  uint64_t reps = 5;
  if (sponsor_status == name("citizen")) reps = 10;

  vouch.emplace(_self, [&](auto& item) {
    item.sponsor = sponsor;
    item.account = account;
    item.reps = reps;
  });

  users.modify(uitra, _self, [&](auto& item) {
    item.reputation += reps;
  });
}

void accounts::punish(name account) {
  require_auth(get_self());
  
  check_user(account);

  auto uitr = users.find(account.value);

  users.modify(uitr, _self, [&](auto& item) {
    item.status = "visitor"_n;
  });

  vouch_tables vouch(get_self(), account.value);

  auto vitr = vouch.begin();

  while (vitr != vouch.end()) {
    auto sponsor = vitr->sponsor;

    auto uitr2 = users.find(sponsor.value);
    users.modify(uitr2, _self, [&](auto& item) {
      item.reputation -= 50;
    });
  }
}

void accounts::vouchreward(name account) {
  check_user(account);

  auto uitr = users.find(account.value);;
  name status = uitr->status;

  vouch_tables vouch(get_self(), account.value);

  auto vitr = vouch.begin();

  while (vitr != vouch.end()) {
    auto sponsor = vitr->sponsor;

    auto uitr2 = users.find(sponsor.value);
    users.modify(uitr2, _self, [&](auto& item) {
      item.reputation += 1;
    });

    vitr++;
  }
}

void accounts::addref(name referrer, name invited)
{
  require_auth(get_self());

  check(is_account(referrer), "wrong referral");
  check(is_account(invited), "wrong invited");

  refs.emplace(_self, [&](auto& ref) {
    ref.referrer = referrer;
    ref.invited = invited;
  });
}


void accounts::addrep(name user, uint64_t amount)
{
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    user.reputation += amount;
  });

  auto ritr = reps.find(user.value);
  if (ritr == reps.end()) {
    reps.emplace(_self, [&](auto& rep) {
      rep.account = user;
      rep.reputation = amount;
    });
  } else {
    reps.modify(ritr, _self, [&](auto& rep) {
      rep.reputation += amount;
    });
  }
}

void accounts::subrep(name user, uint64_t amount)
{
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    if (user.reputation < amount) {
      user.reputation = 0;
    } else {
      user.reputation -= amount;
    }
  });

  auto ritr = reps.find(user.value);

  if (ritr == reps.end()) {
    reps.emplace(_self, [&](auto& rep) {
      rep.account = user;
      rep.reputation = 0;
    });
  } else {
    reps.modify(ritr, _self, [&](auto& rep) {
      if (rep.reputation < amount) {
        rep.reputation = 0;
      } else {
        rep.reputation -= amount;
      }
    });
  }
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

    transaction_tables transactions(contracts::accounts, seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);

    uint64_t invited_users_number = std::distance(refs.lower_bound(user.value), refs.upper_bound(user.value));

    check(bitr->planted.amount >= 50, "user has less than required seeds planted");
    check(titr->transactions_number >= 1, "user has less than required transactions number");
    check(invited_users_number >= 1, "user has less than required referrals");
    check(uitr->reputation >= 100, "user has less than required reputation");

    updatestatus(user, name("resident"));

    vouchreward(user);
}

void accounts::updatestatus(name user, name status)
{
  auto uitr = users.find(user.value);

  check(uitr != users.end(), "updatestatus: user not found - " + user.to_string());

  users.modify(uitr, _self, [&](auto& user) {
    user.status = status;
  });
}

void accounts::makecitizen(name user)
{
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("resident"), "user is not a resident");

    auto bitr = balances.find(user.value);

    transaction_tables transactions(contracts::token, seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);

    uint64_t invited_users_number = std::distance(refs.lower_bound(user.value), refs.upper_bound(user.value));

    check(bitr->planted.amount >= 100, "user has less than required seeds planted");
    check(titr->transactions_number >= 2, "user has less than required transactions number");
    check(invited_users_number >= 3, "user has less than required referrals");
    check(uitr->reputation >= 100, "user has less than required reputation");

    updatestatus(user, name("citizen"));

    vouchreward(user);
}

void accounts::testresident(name user)
{
  require_auth(_self);

  updatestatus(user, name("resident"));

  vouchreward(user);
}

void accounts::testcitizen(name user)
{
  require_auth(_self);

  updatestatus(user, name("citizen"));

  vouchreward(user);
}

void accounts::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}
