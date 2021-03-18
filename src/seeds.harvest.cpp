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

  cs_points_tables biocspoints(get_self(), name("bio").value);
  auto csbioitr = biocspoints.begin();
  while (csbioitr != biocspoints.end()) {
    csbioitr = biocspoints.erase(csbioitr);
  }

  auto bcsitr = biocstemp.begin();
  while (bcsitr != biocstemp.end()) {
    bcsitr = biocstemp.erase(bcsitr);
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
  if (pitr->planted.amount == quantity.amount) {
    planted.erase(pitr);
    size_change(planted_size, -1);
  } else {
    planted.modify(pitr, _self, [&](auto& item) {
      item.planted -= quantity;
    });
  }
  
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

ACTION harvest::updtotal() { // remove when balances are retired
  require_auth(get_self());

  auto bitr = balances.find(_self.value);
  total_table tt = total.get_or_create(get_self(), total_table());
  tt.total_planted = bitr->planted;
  total.set(tt, get_self());
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

// copy everything to harvest table
// ACTION harvest::updharvest(uint64_t startval) {

//   total_table tt = total.get_or_create(get_self(), total_table());
//   tt.total_planted = asset(0, seeds_symbol);
//   total.set(tt, get_self());

//   uint64_t limit = 50;

//   auto bitr = startval == 0 ? balances.begin() : balances.find(startval);

//   while (bitr != balances.end() && limit > 0) {
//     if (bitr->planted.amount > 0 && bitr->account != _self) {
//       planted.emplace(_self, [&](auto& entry) {
//         entry.account = bitr->account;
//         entry.planted = bitr->planted;
//       });
//       size_change(planted_size, 1);
//       change_total(true, bitr->planted);
//     }
//     bitr++;
//     limit--;
//   }

//   if (bitr != balances.end()) {

//     uint64_t next_value = bitr->account.value;
//     action next_execution(
//         permission_level{get_self(), "active"_n},
//         get_self(),
//         "updharvest"_n,
//         std::make_tuple(next_value)
//     );

//     transaction tx;
//     tx.actions.emplace_back(next_execution);
//     tx.delay_sec = 1;
//     tx.send(next_value, _self);

//   } 
// }

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

    uint64_t rank = utils::rank(current, total);

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

    uint64_t rank = utils::rank(current, total);

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
    add_cs_to_bioregion(account, uint32_t(contribution_points));
  }
}

void harvest::add_cs_to_bioregion(name account, uint32_t points) {
  auto bitr = members.find(account.value);
  if (bitr == members.end()) { return; }

  auto csitr = biocstemp.find(bitr -> bioregion.value);
  if (csitr == biocstemp.end()) {
    if (points > 0) {
      biocstemp.emplace(_self, [&](auto & item){
        item.bioregion = bitr -> bioregion;
        item.points = points;
      });
      size_change(cs_bio_size, 1);
    }
  } else {
    if (points > 0) {
      biocstemp.modify(csitr, _self, [&](auto & item){
        item.points += points;
      });
    } else {
      biocstemp.erase(csitr);
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

  while (citr != cs_by_points.end() && count < chunksize) {

    uint64_t rank = utils::rank(current, total);

    cs_by_points.modify(citr, _self, [&](auto& item) {
      item.rank = rank;
    });

    sum_rank += rank;

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


void harvest::rankbiocss() {
  uint64_t batch_size = config_get("batchsize"_n);
  size_set(sum_rank_bios, 0);
  rankbiocs(uint64_t(0), uint64_t(0), batch_size);
}

void harvest::rankbiocs(uint64_t start, uint64_t chunk, uint64_t chunksize) {
  require_auth(get_self());

  uint64_t total = get_size(cs_bio_size);
  if (total == 0) return;

  cs_points_tables biocspoints(get_self(), name("bio").value);

  auto bios_by_points = biocstemp.get_index<"bycspoints"_n>();
  auto bitr = start == 0 ? bios_by_points.begin() : bios_by_points.find(start);
  
  uint64_t current = chunk * chunksize;
  uint64_t count = 0;
  uint64_t sum_rank_b = 0;

  while (bitr != bios_by_points.end() && count < chunksize) {

    uint64_t rank = utils::rank(current, total);

    auto csitr = biocspoints.find(bitr -> bioregion.value);
    if (csitr == biocspoints.end()) {
      biocspoints.emplace(_self, [&](auto & item){
        item.account = bitr -> bioregion;
        item.contribution_points = bitr -> points;
        item.rank = rank;
      });
    } else {
      biocspoints.modify(csitr, _self, [&](auto & item){
        item.contribution_points = bitr -> points;
        item.rank = rank;
      });
    }

    sum_rank_b += rank;

    bitr = bios_by_points.erase(bitr);
    count++;
    current++;
  }

  size_change(sum_rank_bios, int64_t(sum_rank_b));

  if (bitr != bios_by_points.end()) {
    uint64_t next_value = bitr -> by_cs_points();
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "rankbiocs"_n,
      std::make_tuple(next_value, chunk + 1, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  } else {
    size_set(cs_bio_size, 0);
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
  check(qevs.begin() != qevs.end(), "The qevs table for " + contracts::history.to_string() + " is empty");

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

  int64_t target_supply = (1.0 + volume_growth) * previous_qev_itr -> circulating_supply;

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
    std::make_tuple(uint64_t(0), config_get("batchsize"_n), amount)
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

void harvest::runharvest() {
  require_auth(get_self());

  auto mitr = mintrate.begin();
  // check(mitr != mintrate.end(), "mint rate table is empty");
  if (mitr == mintrate.end()) {
    print("mint rate is empty");
    return;
  }

  if (mitr -> mint_rate <= 0) { return; }

  asset quantity = asset(mitr -> mint_rate, test_symbol);
  string memo = "harvest";

  print("mint rate:", quantity, "\n");

  token::issue_action_test t_issue{contracts::token, { contracts::token, "minthrvst"_n }};
  t_issue.send(get_self(), quantity, memo);

  double users_percentage = config_get("hrvst.users"_n) / 1000000.0;
  double bios_percentage = config_get("hrvst.bios"_n) / 1000000.0;
  double orgs_percentage = config_get("hrvst.orgs"_n) / 1000000.0;
  double global_percentage = config_get("hrvst.global"_n) / 1000000.0;

  print("amount for users: ", asset(mitr -> mint_rate * users_percentage, test_symbol), "\n");
  print("amount for bios: ", asset(mitr -> mint_rate * bios_percentage, test_symbol), "\n");
  print("amount for orgs: ", asset(mitr -> mint_rate * orgs_percentage, test_symbol), "\n");
  print("amount for global: ", asset(mitr -> mint_rate * global_percentage, test_symbol), "\n");

  send_distribute_harvest("disthvstusrs"_n, asset(mitr -> mint_rate * users_percentage, test_symbol));
  send_distribute_harvest("disthvstbios"_n, asset(mitr -> mint_rate * bios_percentage, test_symbol));
  send_distribute_harvest("disthvstorgs"_n, asset(mitr -> mint_rate * orgs_percentage, test_symbol));

  withdraw_aux(get_self(), bankaccts::globaldho, asset(mitr -> mint_rate * global_percentage, test_symbol), "harvest");

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
    tx.send(sum_rank_users.value, _self);
  }

}

void harvest::disthvstbios (uint64_t start, uint64_t chunksize, asset total_amount) {
  require_auth(get_self());

  auto bitr = start == 0 ? bioregions.begin() : bioregions.find(start);

  uint64_t number_bioregions = distance(bioregions.begin(), bioregions.end());
  uint64_t count = 0;

  check(number_bioregions > 0, "number of bioregions must be greater than zero");
  double fragment_seeds = total_amount.amount / double(number_bioregions);

  while (bitr != bioregions.end() && count < chunksize) {

    // for the moment, all bioregions have rank 1
    print("bio:", bitr -> id, ", rank:", 1, ", amount:", asset(fragment_seeds, test_symbol), "\n");
    withdraw_aux(get_self(), name(bitr -> id), asset(fragment_seeds, test_symbol), "harvest");

    bitr++;
    count++;
  }

  if (bitr != bioregions.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "disthvstbios"_n,
      std::make_tuple(bitr -> id, chunksize, total_amount)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(sum_rank_bios.value, _self);
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
  
  while (csitr != cspoints_t.end() && count < chunksize) {

    // auto uitr = users.find(csitr -> account.value);
    if (csitr->rank > 0) {

      print("org:", csitr -> account, ", rank:", csitr -> rank, ", amount:", asset(csitr -> rank * fragment_seeds, test_symbol), "\n");
      withdraw_aux(get_self(), csitr -> account, asset(csitr -> rank * fragment_seeds, test_symbol), "harvest");
    
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


ACTION harvest::testmigscope (name account, uint64_t amount) {
  require_auth(get_self());

  auto citr = cspoints.find(account.value);
  if (citr != cspoints.end()) {
    cspoints.modify(citr, _self, [&](auto & item){
      item.contribution_points = amount;
      item.rank = amount;
    });
  } else {
    cspoints.emplace(_self, [&](auto & item){
      item.account = account;
      item.contribution_points = amount;
      item.rank = amount;
    });
  }

}

ACTION harvest::migorgs (uint64_t start) {
  require_auth(get_self());

  cs_points_tables cspoints_org(get_self(), organization_scope.value);

  auto csitr = start == 0 ? cspoints.begin() : cspoints.find(start);

  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;

  while (csitr != cspoints.end() && count < batch_size) {

    auto uitr = users.find(csitr->account.value);
    if (uitr->type == name("organisation")) {

      auto org_itr = cspoints_org.find(uitr->account.value);

      if (org_itr != cspoints_org.end()) {
        cspoints_org.modify(org_itr, _self, [&](auto & item){
          item.contribution_points = csitr->contribution_points;
          item.rank = csitr->rank;
        });
      } else {
        cspoints_org.emplace(_self, [&](auto & item){
          item.account = uitr->account;
          item.contribution_points = csitr->contribution_points;
          item.rank = csitr->rank;
        });
      }

    }

    csitr++;
    count++;
  }

  if (csitr != cspoints.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "migorgs"_n,
      std::make_tuple(csitr->account.value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(csitr->account.value, _self);
  }

}

ACTION harvest::delcsorg (uint64_t start) {
  require_auth(get_self());

  auto csitr = start == 0 ? cspoints.begin() : cspoints.find(start);

  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;

  while (csitr != cspoints.end() && count < batch_size) {
    auto uitr = users.find(csitr->account.value);
    if (uitr->type == name("organisation")) {
      csitr = cspoints.erase(csitr);
    } else {
      csitr++;
    }
    count++;
  }

  if (csitr != cspoints.end()) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "delcsorg"_n,
      std::make_tuple(csitr->account.value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(csitr->account.value, _self);
  }

}


