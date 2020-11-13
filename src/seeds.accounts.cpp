#include <seeds.accounts.hpp>
#include <eosio/system.hpp>
#include <eosio/symbol.hpp>
#include <eosio/transaction.hpp>
#include <harvest_table.hpp>

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

  auto resident_basepoints = config_get(resident_vouch_points);
  auto citizen_basepoints = config_get(citizen_vouch_points);

  uint64_t reps = 0;
  if (sponsor_status == name("resident")) reps = resident_basepoints;
  if (sponsor_status == name("citizen")) reps = citizen_basepoints;

  reps = reps * utils::get_rep_multiplier(sponsor); // REPLACE with local function

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

  auto maxvouch = config_get(max_vouch_points);
  
  check(total_vouch < maxvouch, "user is already fully vouched!");
  if ( (total_vouch + reps) > maxvouch) {
    reps = maxvouch - total_vouch; // limit to max vouch
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

  auto community_building_points = config_get(cbp_param);

  auto citr = cbs.find(referrer.value);
  if (citr != cbs.end()) {
    cbs.modify(citr, _self, [&](auto& item) {
      item.community_building_score += community_building_points;
    });
  } else {
    cbs.emplace(_self, [&](auto& item) {
      item.account = referrer;
      item.community_building_score = community_building_points;
      item.rank = 0;
    });
    size_change("cbs.sz"_n, 1);
  }

  // see if referrer is org or individual (or nobody)
  auto uitr = users.find(referrer.value);
  if (uitr != users.end()) {
    auto user_type = uitr->type;

    if (user_type == "organisation"_n) 
    {
      name org_reward_param = is_citizen ? org_seeds_reward_citizen : org_seeds_reward_resident;
      auto org_seeds_reward = config_get(org_reward_param);
      asset org_quantity(org_seeds_reward, seeds_symbol);

      name amb_reward_param = is_citizen ? ambassador_seeds_reward_citizen : ambassador_seeds_reward_resident;
      auto amb_seeds_reward = config_get(amb_reward_param);
      asset amb_quantity(amb_seeds_reward, seeds_symbol);

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

      // register cbs in the cbsorg table to rank orgs
      action(
        permission_level(contracts::organization, "active"_n),
        contracts::organization,
        "addcbpoints"_n,
        std::make_tuple(referrer, community_building_points)
      ).send();

    } 
    else 
    {
      // Add reputation point +1
      name reputation_reward_param = is_citizen ? reputation_reward_citizen : reputation_reward_resident;
      auto rep_points = config_get(reputation_reward_param);

      name seed_reward_param = is_citizen ? individual_seeds_reward_citizen : individual_seeds_reward_resident;
      auto seeds_reward = config_get(seed_reward_param);
      asset quantity(seeds_reward, seeds_symbol);

      send_addrep(referrer, rep_points);

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
  // Check balance - if the balance runs out, the rewards run out too.
  token_accts accts(contracts::token, bankaccts::referrals.value);
  const auto& acc = accts.get(symbol("SEEDS", 4).code().raw());
  auto rem_balance = acc.balance;
  if (quantity > rem_balance) {
    // Should not fail in case not enough balance
    // check(false, ("DEBUG: not enough balance on "+bankaccts::referrals.to_string()+" = "+rem_balance.to_string()+", required= "+quantity.to_string()).c_str());
    return;
  }

  // Checks the current SEEDS price from tlosto.seeds table
  auto lastprice = pricehistory.rbegin()->seeds_usd;
  auto initialprice = pricehistory.begin()->seeds_usd;
  float rate = (float)lastprice.amount / (float) initialprice.amount;
  asset adjusted_qty(quantity.amount*rate, symbol("SEEDS", 4));
  // check(false, ("DEBUG: lastprice="+lastprice.to_string()+", rate="+std::to_string(rate)+", qty="+quantity.to_string()+", adj="+adjusted_qty.to_string()).c_str());

  send_to_escrow(bankaccts::referrals, beneficiary, adjusted_qty, "referral reward");
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

void accounts::canresident(name user) {
    check_can_make_resident(user);
}

void accounts::makeresident(name user)
{
    check_can_make_resident(user);

    auto new_status = name("resident");

    updatestatus(user, new_status);

    rewards(user, new_status);
    
    history_add_resident(user);
}

bool accounts::check_can_make_resident(name user) {
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("visitor"), "user is not a visitor");

    auto bitr = balances.find(user.value);

    uint64_t invited_users_number = countrefs(user, 0);
    uint64_t min_planted = config_get("res.plant"_n);
    uint64_t min_tx = config_get("res.tx"_n);
    uint64_t min_invited = config_get("res.referred"_n);
    uint64_t min_rep = config_get("res.rep.pt"_n);

    uint64_t total_transactions = num_transactions(user, min_tx);

    uint64_t reputation_points = rep.get(user.value,  "user has less than required reputation. Actual: 0").rep;

    check(bitr->planted.amount >= min_planted, "user has less than required seeds planted");
    check(total_transactions >= min_tx, "resident: user has less than required transactions number has: "+
      std::to_string(total_transactions) + " needed: "+
      std::to_string(min_tx));
    check(invited_users_number >= min_invited, "user has less than required referrals. Required: " + std::to_string(min_invited) + " Actual: " + std::to_string(invited_users_number));
    check(reputation_points >= min_rep, "user has less than required reputation. Required: " + std::to_string(min_rep) + " Actual: " + std::to_string(reputation_points));

    return true;
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

uint64_t accounts::config_get(name key) {
  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    // only create the error message string in error case for efficiency
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}

void accounts::cancitizen(name user) {
  check_can_make_citizen(user);
}

void accounts::makecitizen(name user)
{
    check_can_make_citizen(user);
    
    auto new_status = name("citizen");

    updatestatus(user, new_status);

    auto aitr = actives.find(user.value);
    if (aitr == actives.end()) {
      rewards(user, new_status);
      history_add_citizen(user);
    }

    add_active(user);
}

bool accounts::check_can_make_citizen(name user) {
    auto uitr = users.find(user.value);
    check(uitr != users.end(), "no user");
    check(uitr->status == name("resident"), "user is not a resident");

    auto bitr = balances.find(user.value);

    uint64_t min_planted = config_get("cit.plant"_n);
    uint64_t min_tx = config_get("cit.tx"_n);
    uint64_t min_invited = config_get("cit.referred"_n);
    uint64_t min_residents_invited = config_get("cit.ref.res"_n);
    uint64_t min_rep_score = config_get("cit.rep.sc"_n);
    uint64_t min_account_age = config_get("cit.age"_n);

    uint64_t invited_users_number = countrefs(user, min_residents_invited);
    uint64_t _rep_score = rep_score(user);

    uint64_t total_transactions = num_transactions(user, min_tx);

    check(bitr->planted.amount >= min_planted, "user has less than required seeds planted");
    check(total_transactions >= min_tx, "user has less than required transactions number has: "+
      std::to_string(total_transactions) + " needed: "+
      std::to_string(min_tx));
    check(invited_users_number >= min_invited, "user has less than required referrals. Required: " + std::to_string(min_invited) + " Actual: " + std::to_string(invited_users_number));
    check(_rep_score >= min_rep_score, "user has less than required reputation. Required: " + std::to_string(min_rep_score) + " Actual: " + std::to_string(_rep_score));
    check(uitr->timestamp < eosio::current_time_point().sec_since_epoch() - min_account_age, "User account must be older than 2 cycles");

    return true;
}

void accounts::demotecitizn (name user) {
  require_auth(get_self());
  auto uitr = users.find(user.value);
  check(uitr != users.end(), "User not found");
  check(uitr -> status == "citizen"_n, "The user must be a citizen");
  updatestatus(user, "resident"_n);
}

void accounts::add_active (name user) {
  action(
    permission_level(contracts::proposals, "active"_n),
    contracts::proposals,
    "addactive"_n,
    std::make_tuple(user)
  ).send();
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

  add_active(user);
}

// return number of transactions outgoing, until a limit
uint32_t accounts::num_transactions(name account, uint32_t limit) {
  transaction_tables transactions(contracts::history, account.value);
  auto titr = transactions.begin();
  uint32_t count = 0;
  while(titr != transactions.end() && count < limit) {
    titr++;
    count++;
  }
  return count;
}

void accounts::rankreps() {
  rankrep(0, 0, 200);
}

void accounts::rankrep(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size("rep.sz"_n);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto rep_by_rep = rep.get_index<"byrep"_n>();
  auto ritr = start_val == 0 ? rep_by_rep.begin() : rep_by_rep.lower_bound(start_val);
  uint64_t count = 0;

  while (ritr != rep_by_rep.end() && count < chunksize) {

    uint64_t rank = utils::rank(current, total);

    rep_by_rep.modify(ritr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    ritr++;
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

void accounts::rankcbss() {
  rankcbs(0, 0, 200);
}

void accounts::rankcbs(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size("cbs.sz"_n);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto cbs_by_cbs = cbs.get_index<"bycbs"_n>();
  auto citr = start_val == 0 ? cbs_by_cbs.begin() : cbs_by_cbs.lower_bound(start_val);
  uint64_t count = 0;

  while (citr != cbs_by_cbs.end() && count < chunksize) {

    uint64_t rank = utils::rank(current, total);

    cbs_by_cbs.modify(citr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    citr++;
  }

  if (citr == cbs_by_cbs.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = citr->by_cbs();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankcbs"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value + 1, _self);
    
  }

}

void accounts::add_rep_item(name account, uint64_t reputation) {
  check(reputation > 0, "reputation must be > 0");
  rep.emplace(_self, [&](auto& item) {
    item.account = account;
    item.rep = reputation;
  });
  size_change("rep.sz"_n, 1);
}

void accounts::changesize(name id, int64_t delta) {
  require_auth(get_self());
  size_change(id, delta);
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

uint64_t accounts::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
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
  if (ritr == rep.end()) {
    add_rep_item(user, amount);
  } else {
    rep.modify(ritr, _self, [&](auto& item) {
      item.rep = amount;
    });
  }
}

void accounts::testsetrs(name user, uint64_t amount) {
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto ritr = rep.find(user.value);
  if (ritr == rep.end()) {
    rep.emplace(_self, [&](auto& item) {
      item.account = user;
      item.rank = amount;
    });
    size_change("rep.sz"_n, 1);  
  } else {
    rep.modify(ritr, _self, [&](auto& item) {
      item.rank = amount;
    });
  }
}


void accounts::testsetcbs(name user, uint64_t amount) {
  require_auth(get_self());

  check(is_account(user), "non existing user");

  auto usritr = users.find(user.value);

  auto citr = cbs.find(user.value);
  if (citr == cbs.end()) {
    cbs.emplace(_self, [&](auto& item) {
      item.account = user;
      item.community_building_score = amount;
      item.rank = 0;
    });
    size_change("cbs.sz"_n, 1);
  } else {
    cbs.modify(citr, _self, [&](auto& item) {
      item.community_building_score = amount;
    });
  }

  if (usritr -> type == organization) {
    // register cbs in the cbsorg table to rank orgs
    action(
      permission_level(contracts::organization, "active"_n),
      contracts::organization,
      "addcbpoints"_n,
      std::make_tuple(user, amount)
    ).send();
  }
}

void accounts::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

uint64_t accounts::countrefs(name user, int check_num_residents) 
{
    auto refs_by_referrer = refs.get_index<"byreferrer"_n>();
    if (check_num_residents == 0) {
      return std::distance(refs_by_referrer.lower_bound(user.value), refs_by_referrer.upper_bound(user.value));
    } else {
      uint64_t count = 0;
      int residents = 0;
      auto ritr = refs_by_referrer.lower_bound(user.value);
      while (ritr != refs_by_referrer.end() && ritr->referrer == user) {
        auto uitr = users.find(ritr->invited.value);
        if (uitr != users.end()) {
          if (uitr->status == "resident"_n || uitr->status == "citizen"_n) {
            residents++;
          }
        }
        ritr++;
        count++;
      }
      check(residents >= check_num_residents, "user has not referred enough residents or citizens: "+std::to_string(residents));
      return count;
    }


}

uint64_t accounts::rep_score(name user) 
{

    auto ritr = rep.find(user.value);

    if (ritr == rep.end()) {
      return 0;
    }

    return ritr->rank;
}

