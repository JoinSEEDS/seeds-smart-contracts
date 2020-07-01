#include <seeds.accounts.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>

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

  auto repitr = rep.begin();
  while (repitr != rep.end()) {
    repitr = rep.erase(repitr);
  }

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
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

  size_change("users.sz"_n, 1);

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
  check(amount > 0, "amount must be > 0");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    user.reputation += amount;
  });

  auto ritr = rep.find(user.value);
  if (ritr == rep.end()) {
    add_rep_item(user, amount);
  } else {
    rep.modify(ritr, _self, [&](auto& item) {
      item.rep += amount;
    });
  }

}

void accounts::subrep(name user, uint64_t amount)
{
  require_auth(get_self());

  check(is_account(user), "non existing user");
  check(amount > 0, "amount must be > 0");

  // modify user reputation - deprecated
  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    if (user.reputation < amount) {
      user.reputation = 0;
    } else {
      user.reputation -= amount;
    }
  });

  auto ritr = rep.find(user.value);
  if (ritr != rep.end()) {
    if (ritr->rep > amount) {
      rep.modify(ritr, _self, [&](auto& item) {
        item.rep -= amount;
      });
    } else {
      rep.erase(ritr);
      size_change("rep.sz"_n, -1);
    }
  }

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

    uint64_t min_planted =  100 * 10000;
    uint64_t min_tx =  50;
    uint64_t min_invited =  3;
    uint64_t min_rep_score =  50;
    //uint64_t min_residents = 1; // 1 resident invited - NOT implemented
    //uint64_t min_account_age = 60 * 24 * 60 * 60; // 2 cycles account age - NOT implemented

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

void accounts::migraterep(uint64_t account, uint64_t cycle, uint64_t chunksize) {
  require_auth(_self);
  auto uitr = account == 0 ? users.begin() : users.find(account);
  uint64_t count = 0;
  while (uitr != users.end() && count < chunksize) {
    if (uitr->reputation > 0) {
      auto ritr = rep.find(uitr->account.value);
      if (ritr != rep.end()) {
        rep.modify(ritr, _self, [&](auto& item) {
          item.rep = uitr->reputation;
        });
      } else {
        add_rep_item(uitr->account, uitr->reputation);
      }
    }
    uitr++;
    count++;
  }
  if (uitr == users.end()) {
    // done
    size_set("users.sz"_n, chunksize * cycle + count);
  } else {
    // recursive call
    uint64_t nextaccount = uitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "migraterep"_n,
        std::make_tuple(nextaccount, cycle+1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(nextaccount + 1, _self);
    
  }
}

void accounts::rankreps() {
  rankrep(0, 0, 200);
}

void accounts::rankrep(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  // DEBUG REMOVE THIS - THIS IS SO THE FUNCTION DOESN"T RUN AMOK
  if (chunk > 10) { 
    return;
  }

  uint64_t total = sizes.get("rep.sz"_n.value, "user rep size not set").size;
  uint64_t current = chunk * chunksize;
  auto rep_by_rep = rep.get_index<"byrep"_n>();
  auto ritr = start_val == 0 ? rep_by_rep.begin() : rep_by_rep.lower_bound(start_val);
  uint64_t count = 0;

  while (ritr != rep_by_rep.end() && count < chunksize) {

    uint64_t rank = (current * 100) / total;

    rep_by_rep.modify(ritr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    ritr++;

    //print("current: "+std::to_string(current));
    //print("ct: "+std::to_string(count));
    //print("index: "+std::to_string(ritr->by_rep()));
  }

  if (ritr == rep_by_rep.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = ritr->by_rep();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankrep"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value + 1, _self);
    
  }

}



void accounts::resetrep() {
  require_auth(_self);

  auto ritr = rep.begin();
  while (ritr != rep.end()) {
    ritr = rep.erase(ritr);
  }

  size_set("rep.sz"_n, 0);

}


void accounts::add_rep_item(name account, uint64_t reputation) {
  check(reputation > 0, "reputation must be > 0");
  rep.emplace(_self, [&](auto& item) {
    item.account = account;
    item.rep = reputation;
  });
  size_change("rep.sz"_n, 1);
}

void accounts::size_change(name id, int delta) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = delta;
    });
  } else {
    uint64_t newsize = sitr->size + delta; 
    if (delta < 0) {
      if (sitr->size < -delta) {
        newsize = 0;
      }
    }
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

void accounts::size_set(name id, uint64_t newsize) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = newsize;
    });
  } else {
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
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
  size_change("users.sz"_n, -1);
  
}

void accounts::testsetrep(name user, uint64_t amount) {
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto uitr = users.find(user.value);
  users.modify(uitr, _self, [&](auto& user) {
    user.reputation = amount;
  });


  auto ritr = rep.find(user.value);
  rep.modify(ritr, _self, [&](auto& item) {
    item.rep = amount;
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

