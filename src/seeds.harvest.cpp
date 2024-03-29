#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <seeds.harvest.hpp>

void harvest::reset() {
  require_auth(_self);

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }
  
  name user = name("seedsuserbbb");
  refund_tables refunds(get_self(), user.value);

  auto ritr = refunds.begin();
  while (ritr != refunds.end()) {
    ritr = refunds.erase(ritr);
  }
  
  auto titr = txpoints.begin();
  while (titr != txpoints.end()) {
    titr = txpoints.erase(titr);
  }
  size_set(tx_points_size, 0);

  tx_points_tables orgtxpt(get_self(), "org"_n.value);
  auto otitr = orgtxpt.begin();
  while (otitr != orgtxpt.end()) {
    otitr = orgtxpt.erase(otitr);
  }
  size_set(org_tx_points_size, 0);

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }

  auto pitr = planted.begin();
  while (pitr != planted.end()) {
    pitr = planted.erase(pitr);
  }

  auto qitr = monthlyqevs.begin();
  while (qitr != monthlyqevs.end()) {
    qitr = monthlyqevs.erase(qitr);
  }

  auto csitr = cspoints.begin();
  while (csitr != cspoints.end()) {
    csitr = cspoints.erase(csitr);
  }

  cs_points_tables cs_points_org(get_self(), organization_scope.value);
  auto org_csitr = cs_points_org.begin();
  while (org_csitr != cs_points_org.end()) {
    org_csitr = cs_points_org.erase(org_csitr);
  }

  cs_points_tables rgncspoints(get_self(), name("rgn").value);
  auto csrgnitr = rgncspoints.begin();
  while (csrgnitr != rgncspoints.end()) {
    csrgnitr = rgncspoints.erase(csrgnitr);
  }

  auto bcsitr = regioncstemp.begin();
  while (bcsitr != regioncstemp.end()) {
    bcsitr = regioncstemp.erase(bcsitr);
  }

  total.remove();

  init_balance(_self);
}

void harvest::plant(name from, name to, asset quantity, string memo) {
  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
        to  ==  get_self() &&                     // to here
        quantity.symbol == seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);

    name target = from;

    if (!memo.empty()) {
      std::size_t found = memo.find(string("sow "));
      if (found!=std::string::npos) {
        string target_acct_name = memo.substr (4,string::npos);
        target = name(target_acct_name);
     } else {
        check(false, "invalid memo");
      }
    }

    check_user(target);

    init_balance(target);
    init_balance(_self);

    add_planted(target, quantity);

    _deposit(quantity);
  }
}

void harvest::add_planted(name account, asset quantity) {
  auto bitr = balances.find(account.value);
  balances.modify(bitr, _self, [&](auto& user) {
    user.planted += quantity;
  });
  auto pitr = planted.find(account.value);
  if (pitr == planted.end()) {
    planted.emplace(_self, [&](auto& item) {
      item.account = account;
      item.planted = quantity;
      item.rank = 0;
    });
    size_change(planted_size, 1);
  } else {
    planted.modify(pitr, _self, [&](auto& item) {
      item.planted += quantity;
    });
  }
  
  change_total(true, quantity);

}

void harvest::sub_planted(name account, asset quantity) {
  auto fromitr = balances.find(account.value);
  check(fromitr->planted.amount >= quantity.amount, "not enough planted balance");
  balances.modify(fromitr, _self, [&](auto& user) {
    user.planted -= quantity;
  });

  auto pitr = planted.find(account.value);
  check(pitr != planted.end(), "user has no balance");

  // enforce min plant, except for system contracts - onboarding uses "sow", which unplants
  if (account != contracts::onboarding) {
    uint64_t min_planted = config_get("inv.min.plnt"_n);
    check(pitr->planted.amount - quantity.amount >= min_planted, "Can't unplant last Seeds " + std::to_string(min_planted/10000.0));
  }

  planted.modify(pitr, _self, [&](auto& item) {
    item.planted -= quantity;
  });
  
  change_total(false, quantity);

}

void harvest::sow(name from, name to, asset quantity) {
    require_auth(from);
    check_user(from);
    check_user(to);

    init_balance(from);
    init_balance(to);

    sub_planted(from, quantity);
    add_planted(to, quantity);

}


void harvest::claimrefund(name from, uint64_t request_id) {
  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();
  check(ritr != refunds.end(), "No refund found");

  asset total = asset(0, seeds_symbol);
  name beneficiary = ritr->account;

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;
      if (refund_time < eosio::current_time_point().sec_since_epoch()) {
        total += ritr->amount;
        ritr = refunds.erase(ritr);
      }
      else{
        ritr++;
      }
    } else {
      ritr++;
    }
  }
  if (total.amount > 0) {
    _withdraw(beneficiary, total);
  }
  action(
      permission_level(contracts::history, "active"_n),
      contracts::history,
      "historyentry"_n,
      std::make_tuple(from, string("trackrefund"), total.amount, string(""))
   ).send();
}

void harvest::cancelrefund(name from, uint64_t request_id) {
  require_auth(from);

  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();

  uint64_t totalReplanted = 0;

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      uint32_t refund_time = ritr->request_time + ONE_WEEK * ritr->weeks_delay;

      if (refund_time > eosio::current_time_point().sec_since_epoch()) {
        auto bitr = balances.find(from.value);

        add_planted(from, ritr->amount);

        totalReplanted += ritr->amount.amount;

        ritr = refunds.erase(ritr);
      } else {
        ritr++;
      }
    } else {
      ritr++;
    }
  }

  action(
      permission_level(contracts::history, "active"_n),
      contracts::history,
      "historyentry"_n,
      std::make_tuple(from, string("trackcancel"), totalReplanted, string(""))
   ).send();
}

void harvest::unplant(name from, asset quantity) {
  require_auth(from);
  check_user(from);

  auto bitr = balances.find(from.value);
  check(bitr->planted.amount >= quantity.amount, "can't unplant more than planted!");

  auto oitr = organizations.find(from.value);
  if (oitr != organizations.end()) {
    check(bitr->planted.amount >= quantity.amount + oitr->planted.amount, "organization can not unplant the initial fee");
  }

  uint64_t lastRequestId = 0;
  uint64_t lastRefundId = 0;

  refund_tables refunds(get_self(), from.value);
  if (refunds.begin() != refunds.end()) {
    auto ritr = refunds.end();
    ritr--;
    lastRequestId = ritr->request_id;
    lastRefundId = ritr->refund_id;
  }

  uint64_t fraction = quantity.amount / 12;
  uint64_t remainder = quantity.amount % 12;

  for (uint64_t week = 1; week <= 12; week++) {

    uint64_t amt = fraction;

    if (week == 12) {
      amt = amt + remainder;
    }

    refunds.emplace(_self, [&](auto& refund) {
      refund.request_id = lastRequestId + 1;
      refund.refund_id = lastRefundId + week;
      refund.account = from;
      refund.amount = asset( amt, quantity.symbol );
      refund.weeks_delay = week;
      refund.request_time = eosio::current_time_point().sec_since_epoch();
    });
  }

  sub_planted(from, quantity);

}

ACTION harvest::updatetxpt(name account) {
  require_auth(get_self());
  auto uitr = users.get(account.value, "user not found");
  calc_transaction_points(account, uitr.type);
}

ACTION harvest::updatecs(name account) {
  require_auth(account);
  auto uitr = users.get(account.value, "user not found");
  calc_contribution_score(account, uitr.type);
}

ACTION harvest::calctotal(uint64_t startval) {
  require_auth(get_self());

  uint64_t limit = 300;
  total_table tt = total.get_or_create(get_self(), total_table());
  if (startval == 0) {
    tt.total_planted = asset(0, seeds_symbol);
  }

  auto pitr = startval == 0 ? planted.begin() : planted.find(startval);

  while (pitr != planted.end() && limit > 0) {
    tt.total_planted += pitr->planted;
    pitr++;
    limit--;
  }
  total.set(tt, get_self());

  if (pitr != planted.end()) {

    uint64_t next_value = pitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calctotal"_n,
        std::make_tuple(next_value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);

  } 
}

// Calculate Transaction Points for a single account
// Returns count of iterations
uint32_t harvest::calc_transaction_points(name account, name type) {
  uint64_t now = eosio::current_time_point().sec_since_epoch();
  uint64_t cutoffdate = now - (utils::moon_cycle * config_float_get("cyctrx.trail"_n));

  transaction_points_tables transactions(contracts::history, account.value);

  uint64_t count = 0;
  uint64_t total_points = 0;

  auto titr = transactions.rbegin();
  while (titr != transactions.rend() && titr -> timestamp >= cutoffdate) {

    total_points += titr -> points;

    titr++;
    count++;
  }

  if (type == name("organisation")) {
    setorgtxpt(account, total_points);
  } else {
    auto tx_points_itr = txpoints.find(account.value);

    if (tx_points_itr == txpoints.end()) {
      if (total_points > 0) {
        txpoints.emplace(_self, [&](auto& entry) {
          entry.account = account;
          entry.points = total_points;
        });
        size_change(tx_points_size, 1);
      }
    } else {
      if (total_points > 0) {
        txpoints.modify(tx_points_itr, _self, [&](auto& entry) {
          entry.points = total_points; 
        });
      } else {
        txpoints.erase(tx_points_itr);
        size_change(tx_points_size, -1);
      }
    }
  }

  return count;
}

void harvest::calctrxpts() {
    calctrxpt(0, 0, 400);
}

void harvest::calctrxpt(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  check(chunksize > 0, "chunk size must be > 0");

  uint64_t total = utils::get_users_size();
  auto uitr = start_val == 0 ? users.begin() : users.lower_bound(start_val);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    uint32_t num = calc_transaction_points(uitr->account, uitr->type);
    count += 1 + num;
    uitr++;
  }

  if (uitr == users.end()) {
    // done
  } else {
    uint64_t next_value = uitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calctrxpt"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void harvest::rankorgtxs() {
  ranktx(0, 0, 200, "org"_n);
}

void harvest::ranktxs() {
  ranktx(0, 0, 200, contracts::harvest);
}

void harvest::ranktx(uint64_t start_val, uint64_t chunk, uint64_t chunksize, name table) {
  require_auth(_self);

  auto s = table == "org"_n ? org_tx_points_size : tx_points_size;
  uint64_t total = get_size(s);
  if (total == 0) return;

  tx_points_tables txpoints_table(get_self(), table.value);

  uint64_t current = chunk * chunksize;
  auto txpt_by_points = txpoints_table.get_index<"bypoints"_n>();
  auto titr = start_val == 0 ? txpt_by_points.begin() : txpt_by_points.lower_bound(start_val);
  uint64_t count = 0;

  while (titr != txpt_by_points.end() && count < chunksize) {

    uint64_t rank = utils::spline_rank(current, total);

    txpt_by_points.modify(titr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    titr++;
  }

  if (titr == txpt_by_points.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = titr->by_points();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "ranktx"_n,
        std::make_tuple(next_value, chunk + 1, chunksize, table)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
    
  }

}

void harvest::rankplanteds() {
  rankplanted(0, 0, 200);
}

void harvest::rankplanted(uint128_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  uint64_t total = get_size(planted_size);
  if (total == 0) return;

  uint64_t current = chunk * chunksize;
  auto planted_by_planted = planted.get_index<"byplanted"_n>();
  auto pitr = start_val == 0 ? planted_by_planted.begin() : planted_by_planted.lower_bound(start_val);
  uint64_t count = 0;

  while (pitr != planted_by_planted.end() && count < chunksize) {

    uint64_t rank = utils::spline_rank(current, total);

    planted_by_planted.modify(pitr, _self, [&](auto& item) {
      item.rank = rank;
    });

    current++;
    count++;
    pitr++;
  }

  if (pitr == planted_by_planted.end()) {
    // Done.
  } else {
    // recursive call
    uint128_t next_value = pitr->by_planted();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankplanted"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(pitr->account.value, _self);
    
  }

}

void harvest::calccss() {
  calccs(0, 0, 200);
}

void harvest::calccs(uint64_t start_val, uint64_t chunk, uint64_t chunksize) {
  require_auth(_self);

  check(chunksize > 0, "chunk size must be > 0");

  uint64_t total = utils::get_users_size();
  auto uitr = start_val == 0 ? users.begin() : users.lower_bound(start_val);
  uint64_t count = 0;

  while (uitr != users.end() && count < chunksize) {
    calc_contribution_score(uitr->account, uitr->type);
    count++;
    uitr++;
  }

  if (uitr == users.end()) {
    // done
  } else {
    uint64_t next_value = uitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "calccs"_n,
        std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

// [PS+RT+CB X Rep = Total Contribution Score]
void harvest::calc_contribution_score(name account, name type) {
  uint64_t planted_score = 0;
  uint64_t transactions_score = 0;
  uint64_t community_building_score = 0;
  uint64_t reputation_score = 0;

  auto pitr = planted.find(account.value);
  if (pitr != planted.end()) planted_score = pitr->rank;

  // CS scrore for org needs to be calculated differently
  // Page 71 constitution

  name scope;
  name cs_scope;
  name cs_sz;

  if (type == "organisation"_n) {
    scope = organization_scope;
    cs_scope = scope;
    cs_sz = cs_org_size;
    
    tx_points_tables orgtxpoints(get_self(), "org"_n.value);
    auto titr = orgtxpoints.find(account.value);
    if (titr != orgtxpoints.end()) transactions_score = titr->rank;

  } else {
    scope = individual_scope_accounts;
    cs_scope = individual_scope_harvest;
    cs_sz = cs_size;

    auto titr = txpoints.find(account.value);
    if (titr != txpoints.end()) transactions_score = titr->rank;
  }

  rep_tables rep_t(contracts::accounts, scope.value);
  auto ritr = rep_t.find(account.value);
  if (ritr != rep_t.end()) reputation_score = ritr->rank;

  cbs_tables cbs_t(contracts::accounts, scope.value);
  auto citr = cbs_t.find(account.value);
  if (citr != cbs_t.end()) community_building_score = citr->rank;

  // TODO verify this as correct for the constitution pp 71
  // Orgs need to have different scope for rep
  // Orgs need to have different scope for cbp

  // org Planted seeds ranking for orgs may need to be in a different scope (Seeds 2.0 feature)
  // ORG Total Organisation Contribution Point = (CC+EC) * ORM

  uint64_t contribution_points = ( (planted_score + transactions_score + community_building_score) * reputation_score * 2) / 100;

  cs_points_tables cspoints_t(get_self(), cs_scope.value);

  auto csitr = cspoints_t.find(account.value);
  if (csitr == cspoints_t.end()) {
    if (contribution_points > 0) {
      cspoints_t.emplace(_self, [&](auto& item) {
        item.account = account;
        item.contribution_points = contribution_points;
      });
      size_change(cs_sz, 1);
    }
  } else {
    if (contribution_points > 0) {
      cspoints_t.modify(csitr, _self, [&](auto& item) {
        item.contribution_points = contribution_points;
      });
    } else {
      cspoints_t.erase(csitr);
      size_change(cs_sz, -1);
    }
  }

  if (type != "organisation"_n) {
    add_cs_to_region(account, uint32_t(contribution_points));
  }
}

void harvest::add_cs_to_region(name account, uint32_t points) {
  auto bitr = members.find(account.value);
  if (bitr == members.end()) { return; }

  auto csitr = regioncstemp.find(bitr -> region.value);
  if (csitr == regioncstemp.end()) {
    if (points > 0) {
      regioncstemp.emplace(_self, [&](auto & item){
        item.region = bitr -> region;
        item.points = points;
      });
      size_change(cs_rgn_size, 1);
    }
  } else {
    if (points > 0) {
      regioncstemp.modify(csitr, _self, [&](auto & item){
        item.points += points;
      });
    } else {
      regioncstemp.erase(csitr);
    }
  }
}

void harvest::rankcss() {
  size_set(sum_rank_users, 0);
  rankcs(0, 0, 200, individual_scope_harvest);
}

void harvest::rankorgcss() {
  size_set(sum_rank_orgs, 0);
  rankcs(0, 0, 200, organization_scope);
}

void harvest::rankcs(uint64_t start_val, uint64_t chunk, uint64_t chunksize, name cs_scope) {
  require_auth(_self);

  uint64_t total = 0;
  name sum_rank_name;
  if (cs_scope == individual_scope_harvest) {
    total = get_size(cs_size);
    sum_rank_name = sum_rank_users;
  } else if (cs_scope == organization_scope) {
    total = get_size(cs_org_size);
    sum_rank_name = sum_rank_orgs;
  }
  if (total == 0) return;

  cs_points_tables cspoints_t(get_self(), cs_scope.value);

  uint64_t current = chunk * chunksize;
  auto cs_by_points = cspoints_t.get_index<"bycspoints"_n>();
  auto citr = start_val == 0 ? cs_by_points.begin() : cs_by_points.lower_bound(start_val);
  uint64_t count = 0;
  uint64_t sum_rank = 0;

  uint64_t min_eligible = config_get(name("org.minharv"));

  while (citr != cs_by_points.end() && count < chunksize) {

    uint64_t rank = utils::linear_rank(current, total);

    cs_by_points.modify(citr, _self, [&](auto& item) {
      item.rank = rank;
    });

    if (cs_scope == organization_scope) {
      auto org = organizations.find(citr -> account.value);
      if (org -> status >= min_eligible) {
        sum_rank += rank;    
      }   
    } else {
      sum_rank += rank;    
    }

    current++;
    count++;
    citr++;
  }

  size_change(sum_rank_name, int64_t(sum_rank));

  if (citr == cs_by_points.end()) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = citr->by_cs_points();
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rankcs"_n,
        std::make_tuple(next_value, chunk + 1, chunksize, cs_scope)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
    
  }

}


void harvest::rankrgncss() {
  uint64_t batch_size = config_get("batchsize"_n);
  size_set(sum_rank_rgns, 0);
  rankrgncs(uint64_t(0), uint64_t(0), batch_size);
}

void harvest::rankrgncs(uint64_t start, uint64_t chunk, uint64_t chunksize) {
  require_auth(get_self());

  uint64_t total = get_size(cs_rgn_size);
  if (total == 0) return;

  cs_points_tables rgncspoints(get_self(), name("rgn").value);

  auto rgns_by_points = regioncstemp.get_index<"bycspoints"_n>();
  auto bitr = start == 0 ? rgns_by_points.begin() : rgns_by_points.find(start);
  
  uint64_t current = chunk * chunksize;
  uint64_t count = 0;
  uint64_t sum_rank_b = 0;

  while (bitr != rgns_by_points.end() && count < chunksize) {

    uint64_t rank = utils::linear_rank(current, total);

    auto csitr = rgncspoints.find(bitr -> region.value);
    if (csitr == rgncspoints.end()) {
      rgncspoints.emplace(_self, [&](auto & item){
        item.account = bitr -> region;
        item.contribution_points = bitr -> points;
        item.rank = rank;
      });
    } else {
      rgncspoints.modify(csitr, _self, [&](auto & item){
        item.contribution_points = bitr -> points;
        item.rank = rank;
      });
    }

    sum_rank_b += rank;

    bitr = rgns_by_points.erase(bitr);
    count++;
    current++;
  }

  size_change(sum_rank_rgns, int64_t(sum_rank_b));

  if (bitr != rgns_by_points.end()) {
    uint64_t next_value = bitr -> by_cs_points();
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "rankrgncs"_n,
      std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  } else {
    size_set(cs_rgn_size, 0);
  }

}


void harvest::payforcpu(name account) {
    require_auth(get_self()); // satisfied by payforcpu permission
    require_auth(account);

    auto uitr = users.find(account.value);
    check(uitr != users.end(), "Not a Seeds user!");
}

void harvest::init_balance(name account)
{
  auto bitr = balances.find(account.value);
  if (bitr == balances.end()) {
    balances.emplace(_self, [&](auto& user) {
      user.account = account;
      user.planted = asset(0, seeds_symbol);
      user.reward = asset(0, seeds_symbol);
    });
  }
}

void harvest::check_user(name account)
{
  if (account == contracts::onboarding) {
    return;
  }
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "harvest: no user");
}

void harvest::check_asset(asset quantity)
{
  check(quantity.is_valid(), "invalid asset");
  check(quantity.symbol == seeds_symbol, "invalid asset");
}

void harvest::_deposit(asset quantity)
{
  check_asset(quantity);

  token::transfer_action action{contracts::token, {_self, "active"_n}};
  action.send(_self, contracts::bank, quantity, "");
}

void harvest::_withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::transfer_action action{contracts::token, {contracts::bank, "active"_n}};
  action.send(contracts::bank, beneficiary, quantity, "");
}

void harvest::testclaim(name from, uint64_t request_id, uint64_t sec_rewind) {
  require_auth(get_self());
  refund_tables refunds(get_self(), from.value);

  auto ritr = refunds.begin();
  check(ritr != refunds.end(), "No refund found");

  while (ritr != refunds.end()) {
    if (request_id == ritr->request_id) {
      refunds.modify(ritr, _self, [&](auto& refund) {
        refund.request_time = eosio::current_time_point().sec_since_epoch() - sec_rewind;
      });
    }
    ritr++;
  }
  
}

void harvest::testcspoints(name account, uint64_t contribution_points) {
  require_auth(get_self());

  auto uitr = users.get(account.value, "account not found");
  name scope;
  name cs_sz;

  if (uitr.type == "individual"_n) {
    scope = individual_scope_harvest;
    cs_sz = cs_size;
  } else {
    scope = organization_scope;
    cs_sz = cs_org_size;
  }

  cs_points_tables cspoints_t(get_self(), scope.value);

  auto csitr = cspoints_t.find(account.value);
  if (csitr == cspoints_t.end()) {
    if (contribution_points > 0) {
      cspoints_t.emplace(_self, [&](auto& item) {
        item.account = account;
        item.contribution_points = contribution_points;
      });
      size_change(cs_sz, 1);
    }
  } else {
    if (contribution_points > 0) {
      cspoints_t.modify(csitr, _self, [&](auto& item) {
        item.contribution_points = contribution_points;
      });
    } else {
      cspoints_t.erase(csitr);
      size_change(cs_sz, -1);
    }
  }
}

void harvest::testupdatecs(name account, uint64_t contribution_score) {
  require_auth(get_self());

  auto uitr = users.get(account.value, "account not found");
  name scope;
  name cs_sz;

  if (uitr.type == "individual"_n) {
    scope = individual_scope_harvest;
    cs_sz = cs_size;
  } else {
    scope = organization_scope;
    cs_sz = cs_org_size;
  }

  cs_points_tables cspoints_t(get_self(), scope.value);

  auto csitr = cspoints_t.find(account.value);
  if (csitr == cspoints_t.end()) {
    if (contribution_score > 0) {
      cspoints_t.emplace(_self, [&](auto& item) {
        item.account = account;
        item.rank = contribution_score;
      });
      size_change(cs_sz, 1);
    }
  } else {
    if (contribution_score > 0) {
      cspoints_t.modify(csitr, _self, [&](auto& item) {
        item.rank = contribution_score;
      });
    } else {
      cspoints_t.erase(csitr);
      size_change(cs_sz, -1);
    }
  }
}

double harvest::get_rep_multiplier(name account) {
  //return 1.0;  // DEBUg FOR TESTINg otherwise everyone on testnet has 0
  return utils::get_rep_multiplier(account);
}

void harvest::size_change(name id, int delta) {
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

void harvest::size_set(name id, uint64_t newsize) {
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

uint64_t harvest::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}

void harvest::change_total(bool add, asset quantity) {
  total_table tt = total.get_or_create(get_self(), total_table());
  if (tt.total_planted.amount == 0) {
    tt.total_planted = asset(0, seeds_symbol);
  }
  if (add) {
    tt.total_planted = tt.total_planted + quantity;
  } else {
    tt.total_planted = tt.total_planted - quantity;
  }
  total.set(tt, get_self());
}

ACTION harvest::setorgtxpt(name organization, uint64_t tx_points) {
  require_auth(get_self());

  tx_points_tables orgtxpoints(get_self(), "org"_n.value);
  
  auto oitr = orgtxpoints.find(organization.value);
  if (oitr == orgtxpoints.end()) {
    if (tx_points > 0) {
      orgtxpoints.emplace(_self, [&](auto& item) {
        item.account = organization;
        item.points = tx_points;
      });
      size_change(org_tx_points_size, 1);
    }
  } else {
    if (tx_points > 0) {
      orgtxpoints.modify(oitr, _self, [&](auto& item) {
        item.points = tx_points;
      });
    } else {
      orgtxpoints.erase(oitr);
      size_change(org_tx_points_size, -1);
    }
  } 

}

void harvest::calcmqevs () {
  require_auth(get_self());
  
  uint64_t day = utils::get_beginning_of_day_in_seconds();
  uint64_t cutoff = day - utils::moon_cycle;
  
  qev_tables qevs(contracts::history, contracts::history.value);
  if (qevs.begin() == qevs.end()) {
    print("QEVs table is empty, no op. ");
    return;
  }

  auto qitr = qevs.rbegin();
  uint64_t total_volume = 0;

  while (qitr != qevs.rend() && qitr -> timestamp >= cutoff) {
    total_volume += qitr -> qualifying_volume;
    qitr++;
  }

  circulating_supply_table c = circulating.get();

  auto mqitr = monthlyqevs.find(day);
  
  if (mqitr != monthlyqevs.end()) {
    monthlyqevs.modify(mqitr, _self, [&](auto & item){
      item.qualifying_volume = total_volume;
      item.circulating_supply = c.circulating;
    });
  } else {
    monthlyqevs.emplace(_self, [&](auto & item){
      item.timestamp = day;
      item.qualifying_volume = total_volume;
      item.circulating_supply = c.circulating;
    });
  }
}

void harvest::testcalcmqev (uint64_t day, uint64_t total_volume, uint64_t circulating) {
  require_auth(get_self());
  
  auto mqitr = monthlyqevs.find(day);
  
  if (mqitr != monthlyqevs.end()) {
    monthlyqevs.modify(mqitr, _self, [&](auto & item){
      item.qualifying_volume = total_volume;
      item.circulating_supply = circulating;
    });
  } else {
    monthlyqevs.emplace(_self, [&](auto & item){
      item.timestamp = day;
      item.qualifying_volume = total_volume;
      item.circulating_supply = circulating;
    });
  }
}


void harvest::calcmintrate () {
  require_auth(get_self());

  uint64_t day = utils::get_beginning_of_day_in_seconds();
  auto previous_day_temp = eosio::time_point_sec((day - (3 * utils::moon_cycle)) / 86400 * 86400);
  uint64_t previous_day = previous_day_temp.utc_seconds;

  auto current_qev_itr = monthlyqevs.find(day);
  auto previous_qev_itr = monthlyqevs.find(previous_day);

  if (current_qev_itr == monthlyqevs.end() || previous_qev_itr == monthlyqevs.end()) { return; }

  double volume_growth = double(current_qev_itr -> qualifying_volume - previous_qev_itr -> qualifying_volume) / previous_qev_itr -> qualifying_volume;

  int64_t target_supply_raw = (1.0 + volume_growth) * previous_qev_itr -> circulating_supply;

  double inflation_rate = config_float_get("infation.per"_n);

  int64_t target_supply = (1.0 + inflation_rate) * target_supply_raw;

  int64_t delta = target_supply - current_qev_itr -> circulating_supply;

  double mint_rate = delta / 708.0;

  auto mitr = mintrate.begin();
  if (mitr != mintrate.end()) {
    mintrate.modify(mitr, _self, [&](auto & item){
      item.mint_rate = mint_rate;
      item.volume_growth = volume_growth * 10000;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  } else {
    mintrate.emplace(_self, [&](auto & item){
      item.id = mintrate.available_primary_key();
      item.mint_rate = mint_rate;
      item.volume_growth = volume_growth * 10000;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  }

}

uint64_t harvest::config_get(name key) {
  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}

double harvest::config_float_get(name key) {
  auto citr = configfloat.find(key.value);
  if (citr == configfloat.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}

void harvest::send_distribute_harvest (name key, asset amount) {

  cancel_deferred(key.value);

  action next_execution(
    permission_level{get_self(), "active"_n},
    get_self(),
    key,
    // std::make_tuple(uint64_t(0), config_get("batchsize"_n), amount)
    // Note we had timeouts with high chunk sizes so being very conservative here.
    std::make_tuple(uint64_t(0), uint64_t(20), amount)
  );

  transaction tx;
  tx.actions.emplace_back(next_execution);
  tx.delay_sec = 1;
  tx.send(key.value, _self);

}

void harvest::withdraw_aux (name sender, name beneficiary, asset quantity, string memo) {
  token::transfer_action t_action{contracts::token, { sender, "active"_n }};
  t_action.send(sender, beneficiary, quantity, memo);
}

void harvest::send_pool_payout (asset quantity) {
  action a(
    permission_level(contracts::pool, "hrvst.pool"_n),
    contracts::pool,
    "payouts"_n,
    std::make_tuple(quantity)
  );
  transaction tx;
  tx.actions.emplace_back(a);
  tx.delay_sec = 1;
  tx.send(name("poolpayout").value, _self);
}

void harvest::runharvest() {
  require_auth(get_self());

  auto mitr = mintrate.begin();
  // check(mitr != mintrate.end(), "mint rate table is empty");
  if (mitr == mintrate.end()) {
    print("mint rate is empty");
    return;
  }

  if (mitr -> mint_rate <= 0) { return; }

  asset quantity;
  size_tables pool_sizes_t(contracts::pool, contracts::pool.value);

  auto total_pool_balance_itr = pool_sizes_t.find(name("total.sz").value);
  int64_t pool_payout = 0;

  if (total_pool_balance_itr != pool_sizes_t.end() && total_pool_balance_itr->size > 0) {
    pool_payout = std::min(int64_t(mitr->mint_rate * 0.5), int64_t(total_pool_balance_itr->size));
    send_pool_payout(asset(pool_payout, utils::seeds_symbol));
  }

  quantity = asset(mitr->mint_rate - pool_payout, test_symbol);

  string memo = "harvest";

  print("minted:", quantity, "\n");

  token::mint_action t_issue{contracts::token, { contracts::token, "minthrvst"_n }};
  t_issue.send(get_self(), quantity, memo);

  double users_percentage = config_get("hrvst.users"_n) / 1000000.0;
  double rgns_percentage = config_get("hrvst.rgns"_n) / 1000000.0;
  double orgs_percentage = config_get("hrvst.orgs"_n) / 1000000.0;
  double global_percentage = config_get("hrvst.global"_n) / 1000000.0;

  print("amount for users: ", asset(quantity.amount * users_percentage, test_symbol), "\n");
  print("amount for rgns: ", asset(quantity.amount * rgns_percentage, test_symbol), "\n");
  print("amount for orgs: ", asset(quantity.amount * orgs_percentage, test_symbol), "\n");
  print("amount for global: ", asset(quantity.amount * global_percentage, test_symbol), "\n");

  send_distribute_harvest("disthvstusrs"_n, asset(quantity.amount * users_percentage, test_symbol));
  send_distribute_harvest("disthvstrgns"_n, asset(quantity.amount * rgns_percentage, test_symbol));
  send_distribute_harvest("disthvstorgs"_n, asset(quantity.amount * orgs_percentage, test_symbol));
  send_distribute_harvest("disthvstdhos"_n, asset(quantity.amount * global_percentage, test_symbol));

}

void harvest::disthvstusrs (uint64_t start, uint64_t chunksize, asset total_amount) {
  require_auth(get_self());

  auto csitr = start == 0 ? cspoints.begin() : cspoints.find(start);
  uint64_t count = 0;

  uint64_t sum_rank = get_size(sum_rank_users);
  check(sum_rank > 0, "the sum rank for users must be greater than zero");

  double fragment_seeds = total_amount.amount / double(sum_rank);
  
  while (csitr != cspoints.end() && count < chunksize) {

    // auto uitr = users.find(csitr -> account.value);
    if (csitr->rank > 0) {

      print("user:", csitr->account, ", rank:", csitr -> rank, ", amount:", asset(csitr -> rank * fragment_seeds, test_symbol), "\n");
      withdraw_aux(get_self(), csitr->account, asset(csitr->rank * fragment_seeds, test_symbol), "harvest");
    
    }

    csitr++;
    count++;
  }

  if (csitr != cspoints.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "disthvstusrs"_n,
      std::make_tuple(csitr -> account.value, chunksize, total_amount)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(csitr -> account.value, _self);
  }

}

void harvest::disthvstrgns (uint64_t start, uint64_t chunksize, asset total_amount) {
  require_auth(get_self());

  auto regions_by_status_id = regions.get_index<"bystatusid"_n>();
  uint128_t rid = (uint128_t(rgn_status_active.value) << 64) + start;

  auto ritr = regions_by_status_id.lower_bound(rid);

  size_tables rgn_sizes(contracts::region, contracts::region.value);
  auto sitr = rgn_sizes.get(name("active.sz").value, "active.sz not found in region's sizes");

  uint64_t number_regions = sitr.size;
  uint64_t count = 0;

  check(number_regions > 0, "number of regions must be greater than zero");
  double fragment_seeds = total_amount.amount / double(number_regions);

  while (ritr != regions_by_status_id.end() && ritr->status == rgn_status_active && count < chunksize) {

    // for the moment, all regions have rank 1
    print("rgn:", ritr -> id, ", rank:", 1, ", amount:", asset(fragment_seeds, test_symbol), "\n");
    withdraw_aux(get_self(), contracts::region, asset(fragment_seeds, test_symbol), (ritr -> id).to_string());

    ritr++;
    count++;
  }

  if (ritr != regions_by_status_id.end() && ritr->status == rgn_status_active) {
    uint64_t next = ritr->id.value;
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "disthvstrgns"_n,
      std::make_tuple(next, chunksize, total_amount)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next, _self);
  }

}

void harvest::disthvstorgs (uint64_t start, uint64_t chunksize, asset total_amount) {
  require_auth(get_self());

  cs_points_tables cspoints_t(get_self(), organization_scope.value);

  auto csitr = start == 0 ? cspoints_t.begin() : cspoints_t.find(start);
  uint64_t count = 0;

  uint64_t sum_rank = get_size(sum_rank_orgs);
  check(sum_rank > 0, "the sum rank for organizations must be greater than zero");

  double fragment_seeds = total_amount.amount / double(sum_rank);

  uint64_t min_eligible = config_get(name("org.minharv"));
  
  while (csitr != cspoints_t.end() && count < chunksize) {

    // auto uitr = users.find(csitr -> account.value);
    if (csitr->rank > 0) {
      auto uitr = organizations.find(csitr -> account.value);
      if (uitr -> status >= min_eligible) {
        print("org:", csitr -> account, ", rank:", csitr -> rank, ", amount:", asset(csitr -> rank * fragment_seeds, test_symbol), "\n");
        withdraw_aux(get_self(), csitr -> account, asset(csitr -> rank * fragment_seeds, test_symbol), "harvest");
      }
    }

    csitr++;
    count++;
  }

  if (csitr != cspoints_t.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "disthvstorgs"_n,
      std::make_tuple(csitr -> account.value, chunksize, total_amount)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(sum_rank_orgs.value, _self);
  }
}

void harvest::disthvstdhos (uint64_t start, uint64_t chunksize, asset total_amount) {
  require_auth(get_self());

  dho_share_tables dho_share_t (contracts::dao, contracts::dao.value);

  if (dho_share_t.begin() == dho_share_t.end()) {
    withdraw_aux(get_self(), bankaccts::globaldho, total_amount, "harvest");
    return;
  }

  auto ditr = dho_share_t.lower_bound(start);
  uint64_t count = 0;

  while (ditr != dho_share_t.end() && count < chunksize) {
    withdraw_aux(get_self(), ditr->dho, asset(ditr->dist_percentage * total_amount.amount, test_symbol), "harvest");
    ditr++;
    count++;
  }

  if (ditr != dho_share_t.end()) {
    action next_execution(
      permission_level(get_self(), "active"_n),
      get_self(),
      "disthvstdhos"_n,
      std::make_tuple(ditr->dho.value, chunksize, total_amount)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(name("disthvstdhos").value, _self);
  }

}

ACTION harvest::logaction(uint64_t log_group, name action, string log) {
  require_auth(get_self());

  logs_tables logs_t(get_self(), log_group);

  logs_t.emplace(get_self(), [&](auto & newlog) {
   newlog.id = logs_t.available_primary_key();
   newlog.log_group = log_group;
   newlog.action = action;
   newlog.log = log;
  });
}

void harvest::lgcalcmqevs (logmap log_map) {
  require_auth(get_self());

  lgroup_tables log_group_t(get_self(), get_self().value);
  auto log_group = log_group_t.available_primary_key();

  log_group_t.emplace(get_self(), [&](auto & newlog) {
   newlog.log_group = log_group;
   newlog.action = "calcmqev"_n;
   newlog.creation_date = eosio::current_time_point().sec_since_epoch();
  });

  auto ditr = log_map.find("day"_n);
  auto mcitr = log_map.find("mooncycle"_n);
  
  uint64_t day = ditr != log_map.end() ? std::get<1>(ditr->second) : utils::get_beginning_of_day_in_seconds();
  uint64_t moon_cycle = mcitr != log_map.end() ? std::get<1>(mcitr->second) : utils::moon_cycle;
  uint64_t cutoff = day - moon_cycle;

  logaction(log_group, name("calcmqevs"), "Day: " + std::to_string(day));
  logaction(log_group, name("calcmqevs"), "Moon cicle: " + std::to_string(moon_cycle));
  logaction(log_group, name("calcmqevs"), "Cut off: " + std::to_string(cutoff) + ",   Cut off = Day - Moon Cycle");
  
  qev_tables qevs(contracts::history, contracts::history.value);
  if (qevs.begin() == qevs.end()) {
    logaction(log_group, name("calcmqevs"), "QEVs table is empty, no op.");
    return;
  }

  auto qitr = qevs.rbegin();
  uint64_t total_volume = 0;

  while (qitr != qevs.rend() && qitr -> timestamp >= cutoff) {
    total_volume += qitr->qualifying_volume;
    logaction(log_group, name("calcmqevs"), "Qualifying volume:" + std::to_string(qitr->qualifying_volume) + 
      ", date:" + std::to_string(qitr->timestamp) + ",  Total volume partial: " + std::to_string(total_volume));
    qitr++;
  }

  circulating_supply_table c = circulating.get();

  logaction(log_group, name("calcmqevs"), 
    "Total volume: " + std::to_string(total_volume) + ", Circulating supply:" + std::to_string(c.circulating));

}

void harvest::lgcalmntrte (logmap log_map) {
  require_auth(get_self());

  lgroup_tables log_group_t(get_self(), get_self().value);
  auto log_group = log_group_t.available_primary_key();

  log_group_t.emplace(get_self(), [&](auto & newlog) {
   newlog.log_group = log_group;
   newlog.action = "lgcalmntrte"_n;
   newlog.creation_date = eosio::current_time_point().sec_since_epoch();
  });

  auto pqitr = log_map.find("pqevqvol"_n); // previous qev
  auto cqitr = log_map.find("cqevqvol"_n); // current qev
  auto pcitr = log_map.find("pcsupply"_n); // previous circulating supply
  auto ccitr = log_map.find("ccsupply"_n); // current circulating supply
  auto iritr = log_map.find("inflrate"_n); // inflation rate

  uint64_t day = utils::get_beginning_of_day_in_seconds();
  auto previous_day_temp = eosio::time_point_sec((day - (3 * utils::moon_cycle)) / 86400 * 86400);
  uint64_t previous_day = previous_day_temp.utc_seconds;

  auto current_qev_itr = monthlyqevs.find(day);
  auto previous_qev_itr = monthlyqevs.find(previous_day);

  if (current_qev_itr == monthlyqevs.end() && cqitr == log_map.end()) {
    logaction(log_group, name("calcmintrate"), "Current QEV not found for day " + std::to_string(day) + ", no action will be executed.");
    return;
  }

  if (previous_qev_itr == monthlyqevs.end() && pqitr == log_map.end()) {
    logaction(log_group, name("calcmintrate"), "Previous QEV not found for day " + std::to_string(previous_day) + ", no action will be executed.");
    return;
  }

  uint64_t previous_qv = (pqitr != log_map.end()) ? std::get<uint64_t>(pqitr->second) : previous_qev_itr->qualifying_volume;
  uint64_t current_qv = (cqitr != log_map.end()) ? std::get<uint64_t>(cqitr->second) : current_qev_itr->qualifying_volume;
  uint64_t prev_circulating_supply = (pcitr != log_map.end()) ? std::get<uint64_t>(pcitr->second) : previous_qev_itr->circulating_supply;
  uint64_t curr_circulating_supply = (ccitr != log_map.end()) ? std::get<uint64_t>(ccitr->second) : current_qev_itr->circulating_supply;

  logaction(log_group, name("calcmintrate"), "All the integers shown have 4 decimals of precision");

  logaction(log_group, name("calcmintrate"), "Previous QEV: " + std::to_string(previous_qv));
  logaction(log_group, name("calcmintrate"), "Current QEV: " + std::to_string(current_qv));

  logaction(log_group, name("calcmintrate"), "Previous Circulating Supply: " + std::to_string(prev_circulating_supply));
  logaction(log_group, name("calcmintrate"), "Current Circulating Supply: " + std::to_string(curr_circulating_supply));

  double volume_growth = (double(current_qv - previous_qv)) / double(previous_qv);

  logaction(log_group, name("calcmintrate"), "Volume Growth = " + std::to_string(volume_growth) + ",  Volume_Growth = (Current_QEV - Previous_QEV) / Previous_QEV ");

  int64_t target_supply_raw = (1.0 + volume_growth) * prev_circulating_supply;

  logaction(log_group, name("calcmintrate"), "Target Supply Raw = " + std::to_string(target_supply_raw) + ",  Target_Supply_Raw = (1.0 + Volume_Growth) * Prev_Circulating_Supply");

  double inflation_rate = (iritr  != log_map.end()) ? std::get<double>(iritr->second) : config_float_get("infation.per"_n);

  logaction(log_group, name("calcmintrate"), "Inflation Rate = " + std::to_string(inflation_rate));

  int64_t target_supply = (1.0 + inflation_rate) * target_supply_raw;

  logaction(log_group, name("calcmintrate"), "Target Supply = " + std::to_string(target_supply) + ",  Target_Supply = (1.0 + Inflation_Rate) * Target_Supply_Raw");

  int64_t delta = target_supply - curr_circulating_supply;
  
  logaction(log_group, name("calcmintrate"), "Delta = " + std::to_string(delta) + ",  Delta = Target_Supply - Current_Circulating_Supply");

  double mint_rate = delta / 708.0;

  logaction(log_group, name("calcmintrate"), "Mint Rate = " + std::to_string(int64_t(mint_rate)) + ",  Mint_Rate = Delta / 708.0");
}

void harvest::lgrunhrvst(logmap log_map) {
  require_auth(get_self());

  lgroup_tables log_group_t(get_self(), get_self().value);
  auto log_group = log_group_t.available_primary_key();

  log_group_t.emplace(get_self(), [&](auto & newlog) {
   newlog.log_group = log_group;
   newlog.action = "runharvest"_n;
   newlog.creation_date = eosio::current_time_point().sec_since_epoch();
  });

  auto mritr = log_map.find("mintrate"_n);
  auto pbitr = log_map.find("poolbsize"_n);

  auto upitr = log_map.find("usrsperc"_n);
  auto rpitr = log_map.find("rgnsperc"_n);
  auto opitr = log_map.find("orgsperc"_n);
  auto gpitr = log_map.find("globperc"_n);

  auto bzitr = log_map.find("batchsize"_n);

  auto mitr = mintrate.begin();
  if (mitr == mintrate.end() && mritr == log_map.end()) {
    logaction(log_group, name("runharvest"), "Mint rate is empty");
    return;
  }

  int64_t mint_rate = (mritr != log_map.end()) ? std::get<int64_t>(mritr->second) : mitr->mint_rate;

  logaction(log_group, name("runharvest"), "Mint rate: " + std::to_string(mint_rate));

  if (mint_rate <= 0) { 
    logaction(log_group, name("runharvest"), "Mint rate is less than zero, the harvest will not be distributed");
    return; 
  }

  asset quantity;
  size_tables pool_sizes_t(contracts::pool, contracts::pool.value);

  auto total_pool_balance_itr = pool_sizes_t.find(name("total.sz").value);
  int64_t pool_payout = 0;

  if ( (total_pool_balance_itr != pool_sizes_t.end() && total_pool_balance_itr->size > 0) || pbitr != log_map.end() ) {
    uint64_t poolb_size = (pbitr != log_map.end()) ? std::get<1>(pbitr->second) : total_pool_balance_itr->size;
    pool_payout = std::min(int64_t(mint_rate * 0.5), int64_t(poolb_size));

    logaction(log_group, name("runharvest"), "Total pool balance size: " + std::to_string(poolb_size));
    logaction(log_group, name("runharvest"), "Pool payout: " + asset(pool_payout, utils::seeds_symbol).to_string());
  }

  quantity = asset(mint_rate - pool_payout, test_symbol);

  logaction(log_group, name("runharvest"), "Quantity: " + quantity.to_string());

  string memo = "harvest";

  double users_percentage = (upitr != log_map.end()) ? std::get<2>(upitr->second) : config_get("hrvst.users"_n) / 1000000.0;
  double rgns_percentage = (rpitr != log_map.end()) ? std::get<2>(rpitr->second) : config_get("hrvst.rgns"_n) / 1000000.0;
  double orgs_percentage = (opitr != log_map.end()) ? std::get<2>(opitr->second) : config_get("hrvst.orgs"_n) / 1000000.0;
  double global_percentage = (gpitr != log_map.end()) ? std::get<2>(gpitr->second) : config_get("hrvst.global"_n) / 1000000.0;

  logaction(log_group, name("runharvest"), "Amount for users: " + asset(quantity.amount * users_percentage, test_symbol).to_string() + ", Users percentage: " + std::to_string(users_percentage));
  logaction(log_group, name("runharvest"), "Amount for regions: " + asset(quantity.amount * rgns_percentage, test_symbol).to_string() + ", Rgns percentage: " + std::to_string(rgns_percentage));
  logaction(log_group, name("runharvest"), "Amount for organizations: " + asset(quantity.amount * orgs_percentage, test_symbol).to_string() + ", Orgs percentage: " + std::to_string(orgs_percentage));
  logaction(log_group, name("runharvest"), "Amount for global dho: " + asset(quantity.amount * global_percentage, test_symbol).to_string() + ", Global percentage: " + std::to_string(global_percentage));

  uint64_t batch_size = (bzitr != log_map.end()) ? std::get<1>(bzitr->second) : 20;

  log_send_distribute_harvest("ldsthvstusrs"_n, asset(quantity.amount * users_percentage, test_symbol), log_group, batch_size);
  log_send_distribute_harvest("ldsthvstrgns"_n, asset(quantity.amount * rgns_percentage, test_symbol), log_group, batch_size);
  log_send_distribute_harvest("ldsthvstorgs"_n, asset(quantity.amount * orgs_percentage, test_symbol), log_group, batch_size);

  logaction(log_group, name("runharvest"), "Sending " + asset(quantity.amount * global_percentage, test_symbol).to_string() + " to " + bankaccts::globaldho.to_string());
  
  lrewards_tables rewards_t(get_self(), log_group);

  rewards_t.emplace(_self, [&](auto & item) {
    item.account = bankaccts::globaldho;
    item.account_type = "global"_n;
    item.reward = asset(quantity.amount * global_percentage, test_symbol);
    item.notes = "";
  });
}

void harvest::resetlgroups (uint64_t chunksize) {
  require_auth(get_self());

  lgroup_tables log_groups_t(get_self(), get_self().value);
  auto lgitr = log_groups_t.begin();

  uint64_t count = 0;

  while (lgitr != log_groups_t.end() && count < chunksize) {

    uint64_t log_group = lgitr->log_group;

    action a(
      permission_level{get_self(), "active"_n},
      get_self(),
      "resetlogs"_n,
      std::make_tuple(log_group, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(a);
    tx.delay_sec = 1;
    tx.send(log_group, _self);

    lgitr++;
    count++;
  }

  if (lgitr != log_groups_t.end()) {
    action a(
      permission_level{get_self(), "active"_n},
      get_self(),
      "resetlgroups"_n,
      std::make_tuple(chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(a);
    tx.delay_sec = 1;
    tx.send(lgitr->log_group+1, _self);
  }

}

void harvest::resetlogs (uint64_t log_group, uint64_t chunksize) {
  require_auth(get_self());

  lgroup_tables log_groups_t(get_self(), get_self().value);
  auto lgitr = log_groups_t.require_find(log_group, "log group not found");

  uint64_t count = 0;
  bool erased = false;

  logs_tables logs_t(get_self(), log_group);
  auto litr = logs_t.begin();
  while (litr != logs_t.end() && count < chunksize) {
    litr = logs_t.erase(litr);
    count++;
  }

  lrewards_tables rewards_t(get_self(), log_group);
  auto lritr = rewards_t.begin();
  while (lritr != rewards_t.end() && count < chunksize) {
    lritr = rewards_t.erase(lritr);
    count++;
  }

  if (logs_t.begin() == logs_t.end() && rewards_t.begin() == rewards_t.end()) {
    lgitr = log_groups_t.erase(lgitr);
    erased = true;
  }

  if (!erased) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "resetlogs"_n,
      std::make_tuple(log_group, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(log_group, _self);
  }
}

void harvest::ldsthvstusrs (uint64_t start, uint64_t chunksize, asset total_amount, uint64_t log_group) {
  require_auth(get_self());

  auto csitr = start == 0 ? cspoints.begin() : cspoints.find(start);
  uint64_t count = 0;

  uint64_t sum_rank = get_size(sum_rank_users);

  if (sum_rank <= 0) {
    logaction(log_group, name("disthvstusrs"), "the sum rank for users must be greater than zero");
    return;
  }

  double fragment_seeds = total_amount.amount / double(sum_rank);

  logaction(log_group, name("disthvstusrs"), "Total amount available: " + total_amount.to_string());
  logaction(log_group, name("disthvstusrs"), "Sum Rank: " + std::to_string(sum_rank));
  logaction(log_group, name("disthvstusrs"), "Fragment Seeds: " + std::to_string(fragment_seeds) + ", Fragment_Seeds = Total_Amount_Available / Sum_Rank");
  
  while (csitr != cspoints.end() && count < chunksize) {

    if (csitr->rank > 0) {
      string notes = "User: " + csitr->account.to_string() + ", Rank:" + std::to_string(csitr->rank) + 
        ", Amount: " + asset(csitr -> rank * fragment_seeds, test_symbol).to_string() + 
        ", Fragments Seeds: " + std::to_string(fragment_seeds) + 
        ",  Amount = Rank * Fragments_Seeds, 4 decimals of precision when converting to asset";
      
      logaction(log_group, name("disthvstusrs"), notes);
      lrewards_tables rewards_t(get_self(), log_group);
      
      rewards_t.emplace(_self, [&](auto & item) {
        item.account = csitr->account;
        item.account_type = "user"_n;
        item.reward = asset(csitr->rank * fragment_seeds, test_symbol);
        item.notes = notes;
      });

    } else {
      logaction(log_group, name("disthvstusrs"), "User " + csitr->account.to_string() + " is not eligible");
    }

    csitr++;
    count++;
  }

  if (csitr != cspoints.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "ldsthvstusrs"_n,
      std::make_tuple(csitr->account.value, chunksize, total_amount, log_group)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(csitr->account.value, _self);
  }
}

void harvest::ldsthvstrgns (uint64_t start, uint64_t chunksize, asset total_amount, uint64_t log_group) {
  require_auth(get_self());
  
  auto regions_by_status_id = regions.get_index<"bystatusid"_n>();
  uint128_t rid = (uint128_t(rgn_status_active.value) << 64) + start;

  auto ritr = regions_by_status_id.lower_bound(rid);

  size_tables rgn_sizes(contracts::region, contracts::region.value);
  auto sitr = rgn_sizes.get(name("active.sz").value, "active.sz not found in region's sizes");

  uint64_t number_regions = sitr.size;
  uint64_t count = 0;

  if (number_regions <= 0) {
    logaction(log_group, name("disthvstrgns"), "Number of regions must be greater than zero");
    return;
  }

  double fragment_seeds = total_amount.amount / double(number_regions);

  logaction(log_group, name("disthvstrgns"), "Total amount available: " + total_amount.to_string());
  logaction(log_group, name("disthvstrgns"), "Number of Regions: " + std::to_string(number_regions));
  logaction(log_group, name("disthvstrgns"), "Fragment Seeds: " + std::to_string(fragment_seeds) + ", Fragment_Seeds = Total_Amount_Available / Number_Regions");

  logaction(log_group, name("disthvstrgns"), "For the moment, all regions have Rank 1");

  while (ritr != regions_by_status_id.end() && ritr->status == rgn_status_active && count < chunksize) {
    
    string notes = "Rgn: " + ritr->id.to_string() + ", Rank: 1" + ", Amount: " + 
      asset(fragment_seeds, test_symbol).to_string() + 
      ", Amount = Fragment_Seeds * Rank, 4 decimals of precision when converting to asset";
    
    logaction(log_group, name("disthvstrgns"), notes);
    lrewards_tables rewards_t(get_self(), log_group);
    
    rewards_t.emplace(_self, [&](auto & item) {
      item.account = ritr->id;
      item.account_type = "rgn"_n;
      item.reward = asset(fragment_seeds, test_symbol);
      item.notes = notes;
    });

    ritr++;
    count++;
  }

  if (ritr != regions_by_status_id.end() && ritr->status == rgn_status_active) {
    uint64_t next = ritr->id.value;
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "ldsthvstrgns"_n,
      std::make_tuple(next, chunksize, total_amount, log_group)
    );
    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next, _self);
  }
}

void harvest::ldsthvstorgs (uint64_t start, uint64_t chunksize, asset total_amount, uint64_t log_group) {
  require_auth(get_self());

  cs_points_tables cspoints_t(get_self(), organization_scope.value);

  auto csitr = start == 0 ? cspoints_t.begin() : cspoints_t.find(start);
  uint64_t count = 0;

  uint64_t sum_rank = get_size(sum_rank_orgs);
  check(sum_rank > 0, "the sum rank for organizations must be greater than zero");

  double fragment_seeds = total_amount.amount / double(sum_rank);

  uint64_t min_eligible = config_get(name("org.minharv"));

  logaction(log_group, name("disthvstorgs"), "Total amount available: " + total_amount.to_string());
  logaction(log_group, name("disthvstorgs"), "Sum Rank: " + std::to_string(sum_rank));
  logaction(log_group, name("disthvstorgs"), "Fragment Seeds: " + std::to_string(fragment_seeds) + ", Fragment_Seeds = Total_Amount_Available / Sum_Rank");
  logaction(log_group, name("disthvstorgs"), "Minimum Eligible Org Status: " + std::to_string(min_eligible));

  
  while (csitr != cspoints_t.end() && count < chunksize) {

    if (csitr->rank > 0) {
      auto uitr = organizations.find(csitr -> account.value);
      if (uitr -> status >= min_eligible) {

        string notes = "Org: " + csitr->account.to_string() + ", Rank: " + std::to_string(csitr->rank) + 
          ", Amount: " + asset(csitr -> rank * fragment_seeds, test_symbol).to_string() +
          ", Fragments Seeds: " + std::to_string(fragment_seeds) + 
          ",  Amount = Rank * Fragment_Seeds, 4 decimals of precision when converting to asset";

        logaction(log_group, name("disthvstorgs"), notes);
        lrewards_tables rewards_t(get_self(), log_group);

        rewards_t.emplace(_self, [&](auto & item) {
          item.account = csitr->account;
          item.account_type = "org"_n;
          item.reward = asset(csitr -> rank * fragment_seeds, test_symbol);
          item.notes = notes;
        });
      
      } else {
        logaction(log_group, name("disthvstorgs"), "Organization " + csitr->account.to_string() + 
          " is not eligible, its rank is " + std::to_string(csitr->rank) + " and its status " + std::to_string(uitr->status));
      }
    } else {
      logaction(log_group, name("disthvstorgs"), "Organization " + csitr->account.to_string() + " is not eligible, its rank is 0");
    }

    csitr++;
    count++;
  }

  if (csitr != cspoints_t.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "ldsthvstorgs"_n,
      std::make_tuple(csitr->account.value, chunksize, total_amount, log_group)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(sum_rank_orgs.value, _self);
  }
}

void harvest::log_send_distribute_harvest (name key, asset amount, uint64_t log_group, uint64_t batch_size) {
  cancel_deferred(key.value);

  action next_execution(
    permission_level{get_self(), "active"_n},
    get_self(),
    key,
    std::make_tuple(uint64_t(0), batch_size, amount, log_group)
  );

  transaction tx;
  tx.actions.emplace_back(next_execution);
  tx.delay_sec = 1;
  tx.send(key.value, _self);
}
