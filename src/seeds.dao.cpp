#include <seeds.dao.hpp>

#include <proposals/proposals_factory.hpp>

#include "proposals/proposals_base.cpp"
#include "proposals/referendums_settings.cpp"
#include "proposals/proposal_alliance.cpp"
#include "proposals/proposal_campaign_invite.cpp"
#include "proposals/proposal_milestone.cpp"
#include "proposals/proposal_campaign_funding.cpp"

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

  last_proposal_tables lastprop_t(get_self(), get_self().value);
  auto litr = lastprop_t.begin();
  while (litr != lastprop_t.end()) {
    litr = lastprop_t.erase(litr);
  }

  min_stake_tables minstake_t(get_self(), get_self().value);
  auto msitr = minstake_t.begin();
  while (msitr != minstake_t.end()) {
    msitr = minstake_t.erase(msitr);
  }

  size_tables s_t(get_self(), get_self().value);
  auto sitr = s_t.begin();
  while (sitr != s_t.end()) {
    sitr = s_t.erase(sitr);
  }

  size_tables sv_t(get_self(), ProposalsCommon::vote_scope.value);
  auto svitr = sv_t.begin();
  while (svitr != sv_t.end()) {
    svitr = sv_t.erase(svitr);
  }

  for (int i = 0; i < 30; i++) {
    voted_proposals_tables votedprops_t(get_self(), i);
    auto vitr = votedprops_t.begin();
    while (vitr != votedprops_t.end()) {
      vitr = votedprops_t.erase(vitr);
    }
  }

  dho_tables dho_t(get_self(), get_self().value);
  auto dhoitr = dho_t.begin();
  while (dhoitr != dho_t.end()) {
    dhoitr = dho_t.erase(dhoitr);
  }

  dho_vote_tables dho_vote_t(get_self(), get_self().value);
  auto dhovitr = dho_vote_t.begin();
  while (dhovitr != dho_vote_t.end()) {
    dhovitr = dho_vote_t.erase(dhovitr);
  }

  dho_share_tables dho_share_t(get_self(), get_self().value);
  auto dhositr = dho_share_t.begin();
  while (dhositr != dho_share_t.end()) {
    dhositr = dho_share_t.erase(dhositr);
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
  check_attributes(args);

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, type));

  prop->create(args);

}

ACTION dao::update (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(ritr->creator);
  check_attributes(args);

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  prop->update(args);

}

ACTION dao::cancel (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(pitr->creator);

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, pitr->type));

  prop->cancel(args);

}

ACTION dao::callback (std::map<std::string, VariantValue> & args) {

  require_auth(get_self());

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, pitr->type));

  prop->callback(args);

}

ACTION dao::stake (const name & from, const name & to, const asset & quantity, const string & memo) {
  if ( get_first_receiver() == contracts::token && 
       to == get_self() && 
       quantity.symbol == utils::seeds_symbol ) {
      
    // these ones should be overwriten
    if (from == contracts::onboarding) { return; }
    if (from == bankaccts::campaigns) { return; }

    utils::check_asset(quantity);

    uint64_t proposal_id = 0;

    if (memo.empty()) {
      last_proposal_tables lastprop_t(get_self(), get_self().value);

      auto litr = lastprop_t.require_find(from.value, "no proposals");
      proposal_id = litr->proposal_id;
    } else {
      proposal_id = std::stoi(memo);
    }

    proposal_tables proposals_t(get_self(), get_self().value);
    auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

    std::unique_ptr<Proposal> prop = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, pitr->type));

    std::map<std::string, VariantValue> args = {
      { "from", from },
      { "quantity", quantity },
      { "proposal_id", proposal_id }
    };

    prop->stake(args);

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

ACTION dao::erasepartpts (const uint64_t & active_proposals) {
  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t reward_points = config_get(name("voterep1.ind"));

  uint64_t counter = 0;

  participant_tables participants_t(get_self(), get_self().value);
  auto pitr = participants_t.begin();

  while (pitr != participants_t.end() && counter < batch_size) {
    if (pitr->count == active_proposals && pitr->nonneutral) {
      if (reward_points > 0) {
        send_inline_action(
          permission_level(contracts::accounts, "addrep"_n),
          contracts::accounts, 
          "addrep"_n,
          std::make_tuple(pitr->account, reward_points)
        );
      }
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

ACTION dao::createdho (const name & organization) {

  require_auth(organization);

  organization_tables org_t(contracts::organization, contracts::organization.value);
  auto oitr = org_t.require_find(organization.value, "organization not found");

  dho_tables dhos_t(get_self(), get_self().value);
  auto ditr = dhos_t.find(organization.value);

  if (ditr == dhos_t.end()) {
    dhos_t.emplace(_self, [&](auto & item){
      item.org_name = organization;
      item.points = 0;
    });
  }

}

ACTION dao::removedho (const name & organization) {

  require_auth(get_self());

  dho_tables dhos_t(get_self(), get_self().value);
  auto ditr = dhos_t.find(organization.value);

  if (ditr != dhos_t.end()) {

    size_change(dhos_vote_size, -1 * int64_t(ditr->points));

    dho_share_tables shares_t(get_self(), get_self().value);
    auto sitr = shares_t.find(organization.value);
    if (sitr != shares_t.end()) {
      shares_t.erase(sitr);
    }

    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "removedhovts"_n,
      std::make_tuple(ditr->org_name, uint64_t(0), config_get("batchsize"_n), false)
    );

    dhos_t.erase(ditr);
    
  }

}

ACTION dao::removedhovts (const name & organization, const uint64_t & start, const uint64_t & chunksize, const bool & remove_size) {

  require_auth(get_self());

  dho_vote_tables voted_t(get_self(), get_self().value);
  auto voted_by_dho = voted_t.get_index<"bydhoid"_n>();
  auto vitr = voted_by_dho.lower_bound((uint128_t(organization.value) << 64) + start);

  uint64_t count = 0;
  uint64_t total_removed = 0;

  while (vitr != voted_by_dho.end() && vitr->dho == organization && count < chunksize) {

    total_removed += vitr->points;
    vitr = voted_by_dho.erase(vitr);
    
    count+=2;
  }

  if (remove_size) {
    size_change(dhos_vote_size, -1 * total_removed);
  }

  if (vitr != voted_by_dho.end() && vitr->dho == organization) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "removedhovts"_n,
      std::make_tuple(organization, vitr->vote_id, chunksize, remove_size)
    );
  } else {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "dhocalcdists"_n,
      std::make_tuple()
    );
  }

}

ACTION dao::votedhos (const name & account, std::vector<DhoVote> votes) {
  
  if (has_auth(account)) {
    require_auth(account);
    check(!is_trust_delegated(account, dhos_scope), "voice is delegated, user can not vote by him/herself");
  } else {
    require_auth(get_self());
  }

  check_citizen(account);

  dho_tables dho_t(get_self(), get_self().value);

  dho_vote_tables voted_t(get_self(), get_self().value);
  auto voted_by_account = voted_t.get_index<"byacctid"_n>();
  auto vitr = voted_by_account.lower_bound(uint128_t(account.value) << 64);

  int64_t total_old = 0;

  while (vitr != voted_by_account.end() && vitr->account == account) {

    auto ditr = dho_t.find(vitr->dho.value);

    if (ditr != dho_t.end()) {
      dho_t.modify(ditr, _self, [&](auto & item){
        item.points -= vitr->points;
      });
    }

    print("erasing vote for dho: ", vitr->dho, ", points: ", vitr->points, "\n");

    total_old += vitr->points;
    vitr = voted_by_account.erase(vitr);

  }

  int64_t total_new = 0;
  uint64_t total_percentage = 0;
  uint64_t now = current_time_point().sec_since_epoch();

  cs_points_tables cs_t(contracts::harvest, contracts::harvest.value);
  auto csitr = cs_t.require_find(account.value, "contribution score not found");

  // using the rank so the percentages don't get canceled due to low percentage and low rep multiplier
  uint64_t multiplier = csitr->rank;

  for (auto & vote : votes) {

    uint64_t new_points = vote.points * multiplier;

    auto ditr = dho_t.require_find(vote.dho.value, ("dho " + vote.dho.to_string() + " not found").c_str());

    dho_t.modify(ditr, _self, [&](auto & item){
      item.points += new_points;
    });
    
    voted_t.emplace(_self, [&](auto & item){
      item.vote_id = voted_t.available_primary_key();
      item.account = account;
      item.dho = vote.dho;
      item.points = new_points;
      item.timestamp = now;
    });
    
    total_new += new_points;
    total_percentage += vote.points;

  }

  check(total_percentage == 100, "the total votes have to sum up to 100%");

  print("updating size:\n");
  print("total old: ", total_old, "\n");
  print("total new: ", total_new, "\n");

  size_change(dhos_vote_size, total_new - total_old);

  delegate_trust_tables deltrust_t(get_self(), dhos_scope.value);
  auto deltrusts_by_delegatee_delegator = deltrust_t.get_index<"byddelegator"_n>();
  auto ditr = deltrusts_by_delegatee_delegator.lower_bound(uint128_t(account.value) << 64);

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == account) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "dhomimicvote"_n,
      std::make_tuple(account, uint64_t(0), votes, config_get("batchsize"_n))
    );
  }

}

ACTION dao::dhocleanvts () {

  require_auth(get_self());

  uint64_t cutoff = current_time_point().sec_since_epoch() - config_get("dho.v.recast"_n);
  dhocleanvote(cutoff, config_get("batchsize"_n));

}

ACTION dao::dhocleanvote (const uint64_t & cutoff, const uint64_t & chunksize) {

  require_auth(get_self());

  dho_vote_tables votes_t(get_self(), get_self().value);
  auto votes_by_timestamp = votes_t.get_index<"bytimeid"_n>();
  auto vitr = votes_by_timestamp.begin();

  uint64_t count = 0;
  int64_t total_removed = 0;

  dho_tables dho_t(get_self(), get_self().value);

  while (vitr != votes_by_timestamp.end() && cutoff > vitr->timestamp && count < chunksize) {

    total_removed += vitr->points;

    auto ditr = dho_t.find(vitr->dho.value);
    if (ditr != dho_t.end()) {
      dho_t.modify(ditr, _self, [&](auto & item){
        item.points -= vitr->points;
      });
    }

    vitr = votes_by_timestamp.erase(vitr);
    count++;
  }

  size_change(dhos_vote_size, -1 * total_removed);

  if (vitr != votes_by_timestamp.end() && cutoff > vitr->timestamp) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "dhocleanvote"_n,
      std::make_tuple(cutoff, chunksize)
    );
  }

}

ACTION dao::dhocalcdists () {

  require_auth(get_self());

  dho_tables dho_t(get_self(), get_self().value);
  auto dhos_by_points = dho_t.get_index<"bypointsname"_n>();

  auto ditr = dhos_by_points.rbegin();
  uint64_t min_percentage = config_get("dho.min.per"_n);
  uint64_t total_points = get_size(dhos_vote_size);

  print("total points:", total_points, "\n");

  std::vector<DhoVote> valid_dhos;
  uint64_t total_valid_points = 0;

  if (total_points <= 0) return;

  while (ditr != dhos_by_points.rend() && ((ditr->points * 100) / total_points) >= min_percentage) {

    DhoVote valid_dho;
    valid_dho.dho = ditr->org_name;
    valid_dho.points = ditr->points;

    valid_dhos.push_back(valid_dho);
    total_valid_points += ditr->points;

    ditr++;
  }

  if (total_valid_points == 0) return;

  dho_share_tables shares_t(get_self(), get_self().value);
  
  auto sitr = shares_t.begin();
  while (sitr != shares_t.end()) {
    sitr = shares_t.erase(sitr);
  }

  for (auto & valid_dho : valid_dhos) {
    shares_t.emplace(_self, [&](auto & item){
      item.dho = valid_dho.dho;
      item.total_percentage = double(valid_dho.points) / total_points;
      item.dist_percentage = double(valid_dho.points) / total_valid_points;
    });
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
    uint64_t rep_amount = uint64_t(round(rep * rep_multiplier));

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
      participant.nonneutral = option != ProposalsCommon::neutral && participant.nonneutral;
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

uint64_t dao::calc_voice_needed (const uint64_t & total_voice, const uint64_t & num_proposals) {
  return ceil( total_voice * (get_quorum(num_proposals) / 100.0) - 0.000000001);
}

// quorum as % value - e.g. 90.0 == 90%
double dao::get_quorum(const uint64_t total_proposals) {
  double base_quorum = config_get("quorum.base"_n);
  double quorum_min = config_get("quor.min.pct"_n);
  double quorum_max = config_get("quor.max.pct"_n);

  double quorum = total_proposals ? (double)base_quorum / (double)total_proposals : 0;
  quorum = std::max(quorum_min, quorum);
  return std::min(quorum_max, quorum);
}


bool dao::revert_vote (const name & voter, const uint64_t & proposal_id) {

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "referendum not found");

  votes_tables votes_t(get_self(), proposal_id);
  auto vitr = votes_t.find(voter.value);

  if (vitr != votes_t.end()) {
    check(ritr->stage == ProposalsCommon::stage_active, "referendum is not active");
    check(vitr->favour == true && vitr->amount > 0, "only trust votes can be changed");

    proposals_t.modify(ritr, _self, [&](auto & item){
      item.favour -= vitr->amount;
    });

    votes_t.erase(vitr);

    return true;
  }

  return false;

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

uint64_t dao::get_new_moon (uint64_t timestamp) {

  moon_phases_tables moonphases_t(contracts::scheduler, contracts::scheduler.value);

  auto mitr = moonphases_t.lower_bound(timestamp);

  while (mitr != moonphases_t.end()) {
    if (mitr->phase_name == "New Moon") {
      return mitr->timestamp;
    }
    mitr++;
  }

  return 0;

}

void dao::init_cycle_new_stats () {

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c_t = cycle_t.get();

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);

  uint64_t now_minus_5_days = eosio::current_time_point().sec_since_epoch() - (utils::seconds_per_day * 5);

  uint64_t start_time = get_new_moon(now_minus_5_days);
  uint64_t end_time = get_new_moon(start_time + utils::seconds_per_day);

  cyclestats_t.emplace(_self, [&](auto & item){
    item.propcycle = c_t.propcycle;
    item.start_time = start_time;
    item.end_time = end_time;
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

uint64_t dao::active_cutoff_date () {
  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t prop_cycle_sec = config_get(name("propcyclesec"));
  uint64_t inact_cycles = config_get(name("inact.cyc"));
  return now - (inact_cycles * prop_cycle_sec);
}

void dao::check_citizen (const name & account) {
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

name dao::get_fund_type (const name & fund) {
  if (fund == bankaccts::alliances) {
    return alliance_fund;
  } 
  else if (fund == bankaccts::campaigns) {
    return campaign_fund;
  } 
  else if (fund == bankaccts::milestone) {
    return milestone_fund;
  }
  return "none"_n;
}

uint64_t dao::calc_quorum_base (const uint64_t & propcycle) {

  uint64_t num_cycles = config_get("prop.cyc.qb"_n);
  uint64_t total = 0;
  uint64_t count = 0;

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);
  auto citr = cyclestats_t.find(propcycle);

  if (citr == cyclestats_t.end()) {
    // in case there is no information for this propcycle
    return get_size(user_active_size) * 50 / 2;
  }

  while (count < num_cycles) {

    total += citr->total_voice_cast;
    // total += citr -> num_votes; // uncomment to make it count number of voters
    count++;

    if (citr == cyclestats_t.begin()) {
      break;
    } else {
      citr--;
    }

  }

  return count > 0 ? total / count : 0;
}

void dao::update_cycle_stats_from_proposal (const uint64_t & proposal_id, const name & type, const name & array) {
  
  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c_t = cycle_t.get_or_create(get_self(), cycle_table());

  uint64_t quorum_vote_base = calc_quorum_base(c_t.propcycle - 1);

  cycle_stats_tables cyclestats_t(get_self(), get_self().value);

  auto citr = cyclestats_t.find(c_t.propcycle);

  cyclestats_t.modify(citr, _self, [&](auto & item){
    if (array == ProposalsCommon::stage_active) {
      item.active_props.push_back(proposal_id);
    } else if (array == ProposalsCommon::status_evaluate) {
      item.eval_props.push_back(proposal_id);
    }
  });

  if (array == ProposalsCommon::stage_active) {
    support_level_tables support_t(get_self(), type.value);
    auto sitr = support_t.find(c_t.propcycle);
    check(sitr != support_t.end(), "cycle not found " + std::to_string(c_t.propcycle));
    support_t.modify(sitr, _self, [&](auto & item){
      item.num_proposals += 1;
    });
  }

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

