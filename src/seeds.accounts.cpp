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

  auto cbsitr = cbs.begin();
  while (cbsitr != cbs.end()) {
    cbsitr = cbs.erase(cbsitr);
  }

}

void accounts::history_add_resident(name account) {
  action(
    permission_level{contracts::history, "active"_n},
    contracts::history, "addresident"_n,
    std::make_tuple(account)
  ).send();
}

void accounts::history_add_citizen(name account) {
  action(
    permission_level{contracts::history, "active"_n},
    contracts::history, "addcitizen"_n,
    std::make_tuple(account)
  ).send();
}

void accounts::adduser(name account, string nickname, name type)
{
  require_auth(get_self());
  check(is_account(account), "no account");

  check(type == individual|| type == organization, "Invalid type: "+type.to_string()+" type must be either 'individual' or 'organisation'");

  auto uitr = users.find(account.value);
  check(uitr == users.end(), "existing user");

  users.emplace(_self, [&](auto& user) {
      user.account = account;
      user.status = name("visitor");
      user.reputation = 0;
      user.type = type;
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

  check(sponsor_status == name("citizen") || sponsor_status == name("resident"), "sponsor must be a citizen or resident to vouch.");

  _vouch(sponsor, account);
}

/*
* Internal vouch function
*/
void accounts::_vouch(name sponsor, name account) {

  auto uitrs = users.find(sponsor.value);

  if (uitrs == users.end()) {
    return;
  }

  if (uitrs->type != individual) {
    return; // only individuals can vouch
  }

  check_user(account);
  vouch_tables vouch(get_self(), account.value);

  auto uitra = users.find(account.value);

  name sponsor_status = uitrs->status;

  auto resident_basepoints = config.get(resident_vouch_points.value, "settgs.seeds::config: the res.vouch parameter has not been initialized");
  auto citizen_basepoints = config.get(citizen_vouch_points.value, "settgs.seeds::config: the cit.vouch parameter has not been initialized");

  uint64_t reps = 0;
  if (sponsor_status == name("resident")) reps = resident_basepoints.value;
  if (sponsor_status == name("citizen")) reps = citizen_basepoints.value;

  reps = reps * utils::get_rep_multiplier(sponsor);

  if (reps == 0) {
    // this is called from invite accept - just no op
    return;
  }

  // add up existing vouches
  uint64_t total_vouch = 0;
  auto vitr = vouch.begin();
  while (vitr != vouch.end()) {
    total_vouch += vitr->reps;
    vitr++;
  }

  auto maxvouch = config.get(max_vouch_points.value, "settgs.seeds::config: the maxvouch parameter has not been initialized");
  
  check(total_vouch < maxvouch.value, "user is already fully vouched!");
  if ( (total_vouch + reps) > maxvouch.value) {
    reps = maxvouch.value - total_vouch; // limit to max vouch
  }

  // add vouching table entry
  vouch.emplace(_self, [&](auto& item) {
    item.sponsor = sponsor;
    item.account = account;
    item.reps = reps;
  });

  // add reputation to user
  send_addrep(account, reps);
  
}

void accounts::send_addrep(name user, uint64_t amount) {
    action(
      permission_level{_self, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(user, amount)
  ).send();
}

void accounts::send_subrep(name user, uint64_t amount) {
    action(
      permission_level{_self, "active"_n},
      contracts::accounts, "subrep"_n,
      std::make_tuple(user, amount)
  ).send();
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
    send_subrep(sponsor, 50);
    vitr++;
  }
}

void accounts::rewards(name account, name new_status) {
  vouchreward(account);
  refreward(account, new_status);
}

void accounts::vouchreward(name account) {
  check_user(account);

  auto uitr = users.find(account.value);
  name status = uitr->status;

  vouch_tables vouch(get_self(), account.value);

  auto vitr = vouch.begin();

  while (vitr != vouch.end()) {
    auto sponsor = vitr->sponsor;
    send_addrep(sponsor, 1);
    vitr++;
  }
}

void accounts::requestvouch(name account, name sponsor) {
  require_auth(account);
  check_user(sponsor);
  check_user(account);
  
  // Not implemented

}

name accounts::find_referrer(name account) {
  auto ritr = refs.find(account.value);
  if (ritr != refs.end()) return ritr->referrer;
      
  return not_found;
}

void accounts::refreward(name account, name new_status) {
  check_user(account);

  bool is_citizen = new_status.value == name("citizen").value;
    
  name referrer = find_referrer(account);
  if (referrer == not_found) {
    return;
  }

  // Add community building point +1

  name cbp_param = is_citizen ? cbp_reward_citizen : cbp_reward_resident;

  auto community_building_points = config.get(cbp_param.value, "The community building reward parameter has not been initialized yet.");

  auto citr = cbs.find(referrer.value);
  if (citr != cbs.end()) {
    cbs.modify(citr, _self, [&](auto& item) {
      item.community_building_score += community_building_points.value;
    });
  } else {
    cbs.emplace(_self, [&](auto& item) {
      item.account = referrer;
      item.community_building_score = community_building_points.value;
    });
  }

  // see if referrer is org or individual (or nobody)
  auto uitr = users.find(referrer.value);
  if (uitr != users.end()) {
    auto user_type = uitr->type;

    if (user_type == "organisation"_n) 
    {
      name org_reward_param = is_citizen ? org_seeds_reward_citizen : org_seeds_reward_resident;
      auto org_seeds_reward = config.get(org_reward_param.value, "The seeds reward for orgs parameter has not been initialized yet.");
      asset org_quantity(org_seeds_reward.value, seeds_symbol);

      name amb_reward_param = is_citizen ? ambassador_seeds_reward_citizen : ambassador_seeds_reward_resident;
      auto amb_seeds_reward = config.get(amb_reward_param.value, "The seeds reward for orgs parameter has not been initialized yet.");
      asset amb_quantity(amb_seeds_reward.value, seeds_symbol);

      // send reward to org
      send_reward(referrer, org_quantity);

      // send reward to ambassador if we have one
      name org_owner = find_referrer(referrer);
      if (org_owner != not_found) {
        name ambassador = find_referrer(org_owner);
        if (ambassador != not_found) {
          send_reward(ambassador, amb_quantity);
        }
      }

    } 
    else 
    {
      // Add reputation point +1
      name reputation_reward_param = is_citizen ? reputation_reward_citizen : reputation_reward_resident;
      auto rep_points = config.get(reputation_reward_param.value, "The reputation reward for individuals parameter has not been initialized yet.");

      name seed_reward_param = is_citizen ? individual_seeds_reward_citizen : individual_seeds_reward_resident;
      auto seeds_reward = config.get(seed_reward_param.value, "The seeds reward for individuals parameter has not been initialized yet.");
      asset quantity(seeds_reward.value, seeds_symbol);

      send_addrep(referrer, rep_points.value);

      send_reward(referrer, quantity);
    }
  }


  // if referrer is org, find ambassador and add referral there

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

// internal vouch function
void accounts::invitevouch(name referrer, name invited) 
{
  require_auth(get_self());

  _vouch(referrer, invited);
}

void accounts::addrep(name user, uint64_t amount)
{
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    user.reputation += amount;
  });
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
}

void accounts::update(name user, name type, string nickname, string image, string story, string roles, string skills, string interests)
{
    require_auth(user);

    check(type == individual || type == organization, "invalid type");

    auto uitr = users.find(user.value);

    check(uitr->type == type, "Can't change type - create an org in the org contract.");

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

void accounts::send_reward(name beneficiary, asset quantity)
{

  // TODO: Check balance - if the balance runs out, the rewards run out too.

  send_to_escrow(bankaccts::referrals, beneficiary, quantity, "referral reward");
}

void accounts::send_to_escrow(name fromfund, name recipient, asset quantity, string memo)
{

  action(
      permission_level{fromfund, "active"_n},
      contracts::token, "transfer"_n,
      std::make_tuple(fromfund, contracts::escrow, quantity, memo))
      .send();

  action(
      permission_level{fromfund, "active"_n},
      contracts::escrow, "lock"_n,
      std::make_tuple("event"_n,
                      fromfund,
                      recipient,
                      quantity,
                      "golive"_n,
                      "dao.hypha"_n,
                      time_point(current_time_point().time_since_epoch() +
                                 current_time_point().time_since_epoch()), // long time from now
                      memo))
      .send();
}

void accounts::testreward() {
  require_auth(get_self());

  asset quantity(1, seeds_symbol);

  send_reward("accts.seeds"_n, quantity);

}

void accounts::makeresident(name user)
{
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("visitor"), "user is not a visitor");

    auto bitr = balances.find(user.value);

    transaction_tables transactions(contracts::token, seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);
    uint64_t invited_users_number = countrefs(user);

    uint64_t min_planted =  50 * 10000;
    uint64_t min_tx =  10;
    uint64_t min_invited =  1;
    uint64_t min_rep =  50;

    check(bitr->planted.amount >= min_planted, "user has less than required seeds planted");
    check(titr->total_transactions >= min_tx, "user has less than required transactions number.");
    check(invited_users_number >= min_invited, "user has less than required referrals. Required: " + std::to_string(min_invited) + " Actual: " + std::to_string(invited_users_number));
    check(uitr->reputation >= min_rep, "user has less than required reputation. Required: " + std::to_string(min_rep) + " Actual: " + std::to_string(uitr->reputation));

    auto new_status = name("resident");
    updatestatus(user, new_status);

    rewards(user, new_status);
    
    history_add_resident(user);
}

void accounts::updatestatus(name user, name status)
{
  auto uitr = users.find(user.value);

  check(uitr != users.end(), "updatestatus: user not found - " + user.to_string());
  check(uitr->type == individual, "updatestatus: Only individuals can become residents or citizens");

  users.modify(uitr, _self, [&](auto& user) {
    user.status = status;
  });

  bool trust = status == name("citizen");

  action(
    permission_level{contracts::proposals, "active"_n},
    contracts::proposals, "changetrust"_n,
    std::make_tuple(user, trust)
  ).send();

}

void accounts::makecitizen(name user)
{
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("resident"), "user is not a resident");

    auto bitr = balances.find(user.value);

    transaction_tables transactions(contracts::token, seeds_symbol.code().raw());
    auto titr = transactions.find(user.value);

    uint64_t invited_users_number = countrefs(user);
    uint64_t _rep_score = rep_score(user);

// for Resident-Citizen:
// - 100 planted
// - 50 outgoing transfers
// - 3 friends invited
// - 50 reputation SCORE
// - 1 resident invited
// - 60+ days account age
    uint64_t min_planted =  100 * 10000;
    uint64_t min_tx =  50;
    uint64_t min_invited =  3;
    uint64_t min_rep_score =  50;
    uint64_t min_residents = 1; // NOT implemented
    uint64_t min_account_age = 60 * 24 * 60 * 60; // NOT implemented

    check(bitr->planted.amount >= min_planted, "user has less than required seeds planted");
    check(titr->total_transactions >= min_tx, "user has less than required transactions number.");
    check(invited_users_number >= min_invited, "user has less than required referrals. Required: " + std::to_string(min_invited) + " Actual: " + std::to_string(invited_users_number));
    check(_rep_score >= min_rep_score, "user has less than required reputation. Required: " + std::to_string(min_rep_score) + " Actual: " + std::to_string(_rep_score));

    auto new_status = name("citizen");
    updatestatus(user, new_status);

    rewards(user, new_status);
    
    history_add_citizen(user);
}

void accounts::testresident(name user)
{
  require_auth(_self);

  auto new_status = name("resident");
  updatestatus(user, new_status);

  rewards(user, new_status);
  
  history_add_resident(user);
}

void accounts::testvisitor(name user)
{
  require_auth(_self);

  auto new_status = name("visitor");
  updatestatus(user, new_status);
  
}

void accounts::testcitizen(name user)
{
  require_auth(_self);

  auto new_status = name("citizen");

  updatestatus(user, new_status);

  rewards(user, new_status);
  
  history_add_citizen(user);
}

void accounts::genesis(name user) // Remove this after Golive
{ 
  require_auth(_self);

  testresident(user);
  
  testcitizen(user);

}

void accounts::genesisrep() {
  require_auth(_self);
  auto uitr = users.begin();

  while (uitr != users.end()) {
    if (uitr -> status == "citizen"_n && uitr -> reputation < 100) {
      users.modify(uitr, _self, [&](auto& user) {
        user.reputation = 100;
      });
    }
    uitr++;
  }

}

void accounts::testremove(name user)
{
  require_auth(_self);

  auto uitr = users.find(user.value);

  check(uitr != users.end(), "testremove: user not found - " + user.to_string());

  vouch_tables vouch(get_self(), user.value);
  auto vitr = vouch.begin();
  while (vitr != vouch.end()) {
    if (vitr->account == user) {
      vitr = vouch.erase(vitr);
    } else {
      vitr++;
    }
  }

  action(
    permission_level{contracts::proposals, "active"_n},
    contracts::proposals, "changetrust"_n,
    std::make_tuple(user, false)
  ).send();

  users.erase(uitr);
}

void accounts::testsetrep(name user, uint64_t amount) {
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    user.reputation = amount;
  });
}

void accounts::testsetcbs(name user, uint64_t amount) {
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto citr = cbs.find(user.value);
  if (citr == cbs.end()) {
    cbs.emplace(_self, [&](auto& item) {
      item.account = user;
      item.community_building_score = amount;
    });
  } else {
    cbs.modify(citr, _self, [&](auto& item) {
      item.community_building_score = amount;
    });
  }
}

void accounts::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

uint64_t accounts::countrefs(name user) 
{
    auto refs_by_referrer = refs.get_index<"byreferrer"_n>();

    auto numrefs = std::distance(refs_by_referrer.lower_bound(user.value), refs_by_referrer.upper_bound(user.value));

    return numrefs;

}

uint64_t accounts::rep_score(name user) 
{
    DEFINE_HARVEST_TABLE
    eosio::multi_index<"harvest"_n, harvest_table> harvest(contracts::harvest, contracts::harvest.value);

    auto hitr = harvest.find(user.value);

    if (hitr == harvest.end()) {
      return 0;
    }

    return hitr->reputation_score;
}




