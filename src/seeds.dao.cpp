#include <seeds.dao.hpp>

#include <proposals/proposals_factory.hpp>
#include "proposals/referendums_settings.cpp"

#include "proposals/voice_management.cpp"


ACTION dao::reset () {
  require_auth(get_self());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.begin();
  while (ritr != proposals_t.end()) {

    votes_tables votes_t(get_self(), ritr->proposal_id);
    auto vitr = votes_t.begin();
    while (vitr != votes_t.end()) {
      vitr = votes_t.erase(vitr);
    }

    ritr = proposals_t.erase(ritr);
  }

  proposal_auxiliary_tables propaux_t(get_self(), get_self().value);
  auto raitr = propaux_t.begin();
  while (raitr != propaux_t.end()) {
    raitr = propaux_t.erase(raitr);
  }

  for (auto & s : scopes) {
    voice_tables voice_t(get_self(), s.value);
    auto vitr = voice_t.begin();
    while (vitr != voice_t.end()) {
      vitr = voice_t.erase(vitr);
    }

    delegate_trust_tables delegate_t(get_self(), s.value);
    auto ditr = delegate_t.begin();
    while (ditr != delegate_t.end()) {
      ditr = delegate_t.erase(ditr);
    }
  }

  for (auto & fund_type : fund_types) {
    support_level_tables support_t(get_self(), fund_type.value);
    auto fitr = support_t.begin();
    while (fitr != support_t.end()) {
      fitr = support_t.erase(fitr);
    }
  }

  active_tables actives_t(get_self(), get_self().value);
  auto aitr = actives_t.begin();
  while (aitr != actives_t.end()) {
    aitr = actives_t.erase(aitr);
  }

  participant_tables participants_t(get_self(), get_self().value);
  auto pitr = participants_t.begin();
  while (pitr != participants_t.end()) {
    pitr = participants_t.erase(pitr);
  }

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);
  auto citr = cyclestats_t.begin();
  while (citr != cyclestats_t.end()) {
    citr = cyclestats_t.erase(citr);
  }

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_t.remove();
}

ACTION dao::initcycle (const uint64_t & cycle_id) {
  require_auth(get_self());

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c_t = cycle_t.get_or_create(get_self(), cycle_table());

  c_t.propcycle = cycle_id;
  c_t.t_onperiod = current_time_point().sec_since_epoch();

  cycle_t.set(c_t, get_self());
}

ACTION dao::initsz() {
  require_auth(_self);

  uint64_t cutoff_date = active_cutoff_date();
  int64_t count = 0;

  active_tables actives_t(get_self(), get_self().value);

  auto aitr = actives_t.begin();
  while(aitr != actives_t.end()) {
    if (aitr -> timestamp >= cutoff_date) {
      count++;
    }
    aitr++;
  }
  print("size change "+std::to_string(count));
  size_set(user_active_size, count);
}

ACTION dao::create (std::map<std::string, VariantValue> & args) {

  name creator = std::get<name>(args["creator"]);
  name type = std::get<name>(args["type"]);

  require_auth(creator);
  check_citizen(creator);
  check_attributes(args);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, type));

  ref->create(args);

}

ACTION dao::update (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(ritr->creator);
  check_attributes(args);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  ref->update(args);

}

ACTION dao::cancel (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(ritr->creator);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  ref->cancel(args);

}

ACTION dao::stake (const name & from, const name & to, const asset & quantity, const string & memo) {

  if ( get_first_receiver() == contracts::token && 
       to == get_self() && 
       quantity.symbol == utils::seeds_symbol ) {
      
    utils::check_asset(quantity);

    uint64_t proposal_id = std::stoi(memo);

    proposal_tables proposals_t(get_self(), get_self().value);
    auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

    proposals_t.modify(ritr, _self, [&](auto & item){
      item.staked += quantity;
    });

  }

}

ACTION dao::evaluate (const uint64_t & proposal_id, const uint64_t & propcycle) {

  require_auth(get_self());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  std::map<string, VariantValue> args = {
    { string("proposal_id"), proposal_id },
    { string("propcycle"), propcycle }
  };

  ref->evaluate(args);

}

ACTION dao::onperiod () {

  require_auth(get_self());

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get_or_create(get_self(), cycle_table());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto referendums_by_stage_id = proposals_t.get_index<"bystageid"_n>();

  auto staged_itr = referendums_by_stage_id.lower_bound(uint128_t(ProposalsCommon::stage_staged.value) << 64);
  while (staged_itr != referendums_by_stage_id.end() && staged_itr->stage == ProposalsCommon::stage_staged) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(staged_itr->proposal_id, c.propcycle)
    );
    staged_itr++;
  }

  auto active_itr = referendums_by_stage_id.lower_bound(uint128_t(ProposalsCommon::stage_active.value) << 64);
  while (active_itr != referendums_by_stage_id.end() && active_itr->stage == ProposalsCommon::stage_active) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(active_itr->proposal_id, c.propcycle)
    );
    active_itr++;
  }

  c.propcycle += 1;
  c.t_onperiod = current_time_point().sec_since_epoch();
  cycle_t.set(c, get_self());

  init_cycle_new_stats();

  send_deferred_transaction(
    permission_level(get_self(), "active"_n),
    get_self(),
    "updatevoices"_n,
    std::make_tuple()
  );

  uint64_t number_active_proposals = get_size(prop_active_size);

  send_deferred_transaction(
    permission_level(get_self(), "active"_n),
    get_self(),
    "erasepartpts"_n,
    std::make_tuple(number_active_proposals)
  );

}



// ==================================================================== //
// ACTIVE //


ACTION dao::addactive (const name & account) {
  require_auth(get_self());

  active_tables actives_t(get_self(), get_self().value);
  auto aitr = actives_t.find(account.value);
  
  if (aitr == actives_t.end()) {
    actives_t.emplace(_self, [&](auto & a){
      a.account = account;
      a.timestamp = eosio::current_time_point().sec_since_epoch();
    });
    size_change(user_active_size, 1);
    recover_voice(account); // this action is repeated twice when an account becomes citizen
  }
}


// ==================================================================== //





// ==================================================================== //
// PARTICIPANTS //

ACTION dao::erasepartpts (const uint64_t & active_proposals) {
  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t reward_points = config_get(name("voterep1.ind"));

  uint64_t counter = 0;

  participant_tables participants_t(get_self(), get_self().value);
  auto pitr = participants_t.begin();

  while (pitr != participants_t.end() && counter < batch_size) {
    if (pitr->count == active_proposals && pitr->nonneutral) {
      send_inline_action(
        permission_level(contracts::accounts, "addrep"_n),
        contracts::accounts, 
        "addrep"_n,
        std::make_tuple(pitr->account, reward_points)
      );
    }
    counter += 1;
    pitr = participants_t.erase(pitr);
  }

  if (pitr != participants_t.end()) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "erasepartpts"_n,
      std::make_tuple(active_proposals)
    );
  }
}


// ==================================================================== //





// ==================================================================== //
// VOICE //

ACTION dao::favour (const name & voter, const uint64_t & proposal_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, proposal_id, amount, ProposalsCommon::trust, false);
}

ACTION dao::against (const name & voter, const uint64_t & proposal_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, proposal_id, amount, ProposalsCommon::distrust, false);
}

ACTION dao::neutral (const name & voter, const uint64_t & proposal_id) {
  require_auth(voter);
  vote_aux(voter, proposal_id, uint64_t(0), ProposalsCommon::neutral, false);
}

ACTION dao::revertvote (const name & voter, const uint64_t & proposal_id) {

  require_auth( has_auth(voter) ? voter : get_self() );
  
  proposal_tables proposals_t(get_self(), get_self().value);
  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  votes_tables votes_t(get_self(), proposal_id);
  auto vitr = votes_t.require_find(voter.value, "voter has not voted on this proposal, can't revert");

  uint64_t amount = vitr->amount;

  check(pitr->stage == ProposalsCommon::stage_active, "proposal is not in stage active");
  check(vitr->favour == true && amount > 0, "only trust votes can be changed");
  
  votes_t.modify(vitr, _self, [&](auto& item) {
    item.favour = false;
  });

  proposals_t.modify(pitr, _self, [&](auto& item) {
    item.against += amount;
    item.favour -= amount;
  });

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, pitr->type));
  name scope = prop->get_scope();

  if (has_delegates(voter, scope)) {
    send_inline_action(
      permission_level(get_self(), "active"_n),
      get_self(), 
      "mimicrevert"_n,
      std::make_tuple(voter, (uint64_t)0, scope, proposal_id, (uint64_t)30)
    );
  }

}

void dao::vote_aux (const name & voter, const uint64_t & proposal_id, const uint64_t & amount, const name & option, const bool & is_delegated) {

  proposal_tables proposals_t(get_self(), get_self().value);
  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  votes_tables votes_t(get_self(), proposal_id);
  auto vitr = votes_t.find(voter.value);
  check(vitr == votes_t.end(), "only one vote");

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, pitr->type));
  prop->check_can_vote(pitr->status, pitr->stage);

  proposals_t.modify(pitr, _self, [&](auto & item){
    if (option == ProposalsCommon::trust) {
      item.favour += amount;  
    } else if (option == ProposalsCommon::distrust) {
      item.against += amount;
    }
  });

  votes_t.emplace(_self, [&](auto & item){
    item.proposal_id = proposal_id;
    item.account = voter;
    item.amount = amount;
    item.favour = option == ProposalsCommon::trust;
  });

  // storing the number of voters per proposal in a separate scope
  size_tables sizes_t(get_self(), ProposalsCommon::vote_scope.value);
  auto sitr = sizes_t.find(proposal_id);

  if (sitr == sizes_t.end()) {
    sizes_t.emplace(_self, [&](auto & item){
      item.id = name(proposal_id);
      item.size = 1;
    });
  } else {
    sizes_t.modify(sitr, _self, [&](auto & item){
      item.size += 1;
    });
  }

  // reduce voice
  name scope = prop->get_scope();
  double percenetage_used = voice_change(voter, amount, true, scope);

  if (!is_delegated) {
    check(!is_trust_delegated(voter, scope), "voice is delegated, user can not vote by itself");
  }

  send_mimic_delegatee_vote(voter, scope, proposal_id, percenetage_used, option);

  auto rep = config_get(name("voterep2.ind"));
  double rep_multiplier = is_delegated ? config_get(name("votedel.mul")) / 100.0 : 1.0;

  participant_tables participants_t(get_self(), get_self().value);
  auto paitr = participants_t.find(voter.value);

  if (paitr == participants_t.end()) {
    // add reputation for entering in the table
    uint64_t rep_amount = uint64_t(rep * rep_multiplier);

    if (rep_amount > 0) {
      send_inline_action(
        permission_level(contracts::accounts, "addrep"_n),
        contracts::accounts, "addrep"_n,
        std::make_tuple(voter, rep_amount)
      );
    }
    // add the voter to the table
    participants_t.emplace(_self, [&](auto & participant){
      participant.account = voter;
      participant.nonneutral = option != ProposalsCommon::neutral;
      participant.count = 1;
    });

  } else {
    participants_t.modify(paitr, _self, [&](auto & participant){
      participant.count += 1;
      if (option != ProposalsCommon::neutral) { // here, I think this is a bug
        participant.nonneutral = true;
      }
    });
  }

  active_tables actives_t(get_self(), get_self().value);
  auto aitr = actives_t.find(voter.value);
  if (aitr == actives_t.end()) {
    actives_t.emplace(_self, [&](auto& item) {
      item.account = voter;
      item.timestamp = current_time_point().sec_since_epoch();
    });
    size_change(user_active_size, 1);
  } else {
    actives_t.modify(aitr, _self, [&](auto & item){
      item.timestamp = current_time_point().sec_since_epoch();
    });
  }

  add_voted_proposal(proposal_id);

  // this one, maybe it should be called as a callback in the proposal's implementation?
  // because not all proposals increase the voice cast, currently only the ones that are funded
  // have an entry in the support table
  increase_voice_cast(amount, option, prop->get_fund_type());

}

void dao::increase_voice_cast (const uint64_t & amount, const name & option, const name & prop_type) {

  if (prop_type == ProposalsCommon::fund_type_none) { return; }

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get();

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);

  auto citr = cyclestats_t.find(c.propcycle);

  if (citr != cyclestats_t.end()) {
    cyclestats_t.modify(citr, _self, [&](auto & item){
      item.total_voice_cast += amount;
      if (option == ProposalsCommon::trust) {
        item.total_favour += amount;
      } else if (option == ProposalsCommon::distrust) {
        item.total_against += amount;
      }
      item.num_votes += 1;
    });
  }
  add_voice_cast(c.propcycle, amount, prop_type);

}

void dao::add_voted_proposal (const uint64_t & proposal_id) {

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get();

  voted_proposals_tables votedprops_t(get_self(), c.propcycle);
  auto vpitr = votedprops_t.find(proposal_id);

  if (vpitr == votedprops_t.end()) {
    votedprops_t.emplace(_self, [&](auto & prop){
      prop.proposal_id = proposal_id;
    });
  }

}

void dao::add_voice_cast (const uint64_t & cycle, const uint64_t & voice_cast, const name & type) {

  support_level_tables support_t(get_self(), type.value);
  auto sitr = support_t.find(cycle);
  check(sitr != support_t.end(), "cycle not found "+std::to_string(cycle));

  support_t.modify(sitr, _self, [&](auto & item){
    uint64_t total_voice = item.total_voice_cast + voice_cast;
    item.total_voice_cast = total_voice;
    item.voice_needed = calc_voice_needed(total_voice, item.num_proposals);
  });

}

bool dao::is_trust_delegated (const name & account, const name & scope) {
  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto ditr = deltrust_t.find(account.value);
  return ditr != deltrust_t.end();
}

void dao::send_mimic_delegatee_vote (const name & delegatee, const name & scope, const uint64_t & proposal_id, const double & percentage_used, const name & option) {

  uint64_t batch_size = config_get("batchsize"_n);

  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto deltrust_by_delegatee_delegator = deltrust_t.get_index<"byddelegator"_n>();

  uint128_t id = uint128_t(delegatee.value) << 64;
  auto ditr = deltrust_by_delegatee_delegator.lower_bound(id);

  if (ditr != deltrust_by_delegatee_delegator.end() && ditr->delegatee == delegatee) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "mimicvote"_n,
      std::make_tuple(delegatee, ditr->delegator, scope, proposal_id, percentage_used, option, batch_size)
    );
  }

}


// ==================================================================== //

void dao::init_cycle_new_stats () {

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c_t = cycle_t.get();

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);

  cyclestats_t.emplace(_self, [&](auto & item){
    item.propcycle = c_t.propcycle;
    item.start_time = c_t.t_onperiod;
    item.end_time = c_t.t_onperiod + config_get("propcyclesec"_n);
    // item.num_proposals = 0;
    item.num_votes = 0;
    item.total_voice_cast = 0;
    item.total_favour = 0;
    item.total_against = 0;
    item.total_citizens = get_size("voice.sz"_n);
    // item.quorum_vote_base = 0;
    // item.quorum_votes_needed = 0;
    item.unity_needed = double(config_get("propmajority"_n)) / 100.0;
    item.total_eligible_voters = 0;
  });

  for (auto & fund_type : fund_types) {
    uint64_t votes_needed = calc_voice_needed(0, 0);

    support_level_tables support_t(get_self(), fund_type.value);
    auto sitr = support_t.find(c_t.propcycle);

    if (sitr != support_t.end()) {
      support_t.modify(sitr, _self, [&](auto & item){
        item.num_proposals = 0;
        item.total_voice_cast = 0;
        item.voice_needed = votes_needed;
      });
    } else {
      support_t.emplace(_self, [&](auto & item){
        item.propcycle = c_t.propcycle;
        item.num_proposals = 0;
        item.total_voice_cast = 0;
        item.voice_needed = votes_needed;
      });
    }
  }

}

uint64_t dao::calc_voice_needed (const uint64_t & total_voice, const uint64_t & num_proposals) {
  return ceil(total_voice * (get_quorum(num_proposals) / 100.0));
}

uint64_t dao::get_quorum (const uint64_t & total_proposals) {
  uint64_t base_quorum = config_get("quorum.base"_n);
  uint64_t quorum_min = config_get("quor.min.pct"_n);
  uint64_t quorum_max = config_get("quor.max.pct"_n);

  uint64_t quorum = total_proposals ? base_quorum / total_proposals : 0;
  quorum = std::max(quorum_min, quorum);
  return std::min(quorum_max, quorum);
}

uint64_t dao::active_cutoff_date () {
  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t prop_cycle_sec = config_get(name("propcyclesec"));
  uint64_t inact_cycles = config_get(name("inact.cyc"));
  return now - (inact_cycles * prop_cycle_sec);
}

void dao::check_citizen (const name & account) {
  DEFINE_USER_TABLE;
  DEFINE_USER_TABLE_MULTI_INDEX;
  user_tables users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(account.value);
  check(uitr != users.end(), "user not found");
  check(uitr->status == name("citizen"), "user is not a citizen");
}

void dao::check_attributes (const std::map<std::string, VariantValue> & args) {

  string title = std::get<string>(args.at("title"));
  string summary = std::get<string>(args.at("summary"));
  string description = std::get<string>(args.at("description"));
  string image = std::get<string>(args.at("image"));
  string url = std::get<string>(args.at("url"));

  check(title.size() <= 128, "title must be less or equal to 128 characters long");
  check(title.size() > 0, "must have a title");

  check(summary.size() <= 1024, "summary must be less or equal to 1024 characters long");

  check(description.size() > 0, "must have description");

  check(image.size() <= 512, "image url must be less or equal to 512 characters long");
  check(image.size() > 0, "must have image");

  check(url.size() <= 512, "url must be less or equal to 512 characters long");
  
}

template <typename... T>
void dao::send_inline_action (
  const permission_level & permission, 
  const name & contract, 
  const name & action, 
  const std::tuple<T...> & data) {

  eosio::action(permission, contract, action, data).send();

}

template <typename... T>
void dao::send_deferred_transaction (
  const permission_level & permission,
  const name & contract, 
  const name & action,  
  const std::tuple<T...> & data) {

  // eosio::action(permission, contract, action, data).send();

  deferred_id_tables deferredids(get_self(), get_self().value);
  deferred_id_table d_s = deferredids.get_or_create(get_self(), deferred_id_table());

  d_s.id += 1;

  deferredids.set(d_s, get_self());

  transaction trx{};

  trx.actions.emplace_back(
    permission,
    contract,
    action,
    data
  );

  trx.delay_sec = 1;
  trx.send(d_s.id, _self);

}

