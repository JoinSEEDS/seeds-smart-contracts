#include <seeds.referendums.hpp>
#include <proposals/proposals_factory.hpp>

#include "proposals/referendums_settings.cpp"


ACTION referendums::reset () {
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

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_t.remove();
}

ACTION referendums::initcycle (const uint64_t & cycle_id) {
  require_auth(get_self());

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c_t = cycle_t.get_or_create(get_self(), cycle_table());

  c_t.propcycle = cycle_id;
  c_t.t_onperiod = current_time_point().sec_since_epoch();

  cycle_t.set(c_t, get_self());
}

ACTION referendums::initsz() {
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

ACTION referendums::create (std::map<std::string, VariantValue> & args) {

  name creator = std::get<name>(args["creator"]);
  name type = std::get<name>(args["type"]);

  require_auth(creator);
  check_citizen(creator);
  check_attributes(args);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, type));

  ref->create(args);

}

ACTION referendums::update (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(ritr->creator);
  check_attributes(args);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  ref->update(args);

}

ACTION referendums::cancel (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  require_auth(ritr->creator);

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  ref->cancel(args);

}

ACTION referendums::stake (const name & from, const name & to, const asset & quantity, const string & memo) {

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

ACTION referendums::evaluate (const uint64_t & proposal_id) {

  require_auth(get_self());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto ritr = proposals_t.require_find(proposal_id, "proposal not found");

  std::unique_ptr<Proposal> ref = std::unique_ptr<Proposal>(ProposalsFactory::Factory(*this, ritr->type));

  std::map<string, VariantValue> args = {
    { string("proposal_id"), proposal_id }
  };

  ref->evaluate(args);

}

ACTION referendums::onperiod () {

  require_auth(get_self());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto referendums_by_stage_id = proposals_t.get_index<"bystageid"_n>();

  print("running onperiod\n");

  auto staged_itr = referendums_by_stage_id.lower_bound(uint128_t(ProposalsCommon::stage_staged.value) << 64);
  while (staged_itr != referendums_by_stage_id.end() && staged_itr->stage == ProposalsCommon::stage_staged) {
    print("referendum:", staged_itr->proposal_id, "\n");
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(staged_itr->proposal_id)
    );
    staged_itr++;
  }

  auto active_itr = referendums_by_stage_id.lower_bound(uint128_t(ProposalsCommon::stage_active.value) << 64);
  while (active_itr != referendums_by_stage_id.end() && active_itr->stage == ProposalsCommon::stage_active) {
    print("active referendum:", active_itr->proposal_id, "\n");
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(active_itr->proposal_id)
    );
    active_itr++;
  }

}



// ==================================================================== //
// ACTIVE //


ACTION referendums::addactive (const name & account) {
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





// ==================================================================== //





// ==================================================================== //
// VOICE //

ACTION referendums::decayvoices () {
  require_auth(get_self());

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get_or_create(get_self(), cycle_table());

  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t decay_time = config_get(name("decaytime"));
  uint64_t decay_sec = config_get(name("propdecaysec"));

  if ((c.t_onperiod < now)
      && (now - c.t_onperiod >= decay_time)
      && (now - c.t_voicedecay >= decay_sec)
  ) {
    c.t_voicedecay = now;
    cycle_t.set(c, get_self());
    uint64_t batch_size = config_get(name("batchsize"));
    decayvoice(0, batch_size);
  }
}

ACTION referendums::decayvoice (const uint64_t & start, const uint64_t & chunksize) {
  require_auth(get_self());

  voice_tables voices(get_self(), campaign_scope.value);

  uint64_t percentage_decay = config_get(name("vdecayprntge"));
  check(percentage_decay <= 100, "Voice decay parameter can not be more than 100%.");
  
  auto vitr = voices.lower_bound(start);
  uint64_t count = 0;

  double multiplier = (100.0 - (double)percentage_decay) / 100.0;

  while (vitr != voices.end() && count < chunksize) {

    for (auto & s : scopes) {
      voice_tables voice_t(get_self(), s.value);
      auto voice_itr = voice_t.find(vitr->account.value);

      if (voice_itr != voice_t.end()) {
        voice_t.modify(vitr, _self, [&](auto & v){
          v.balance *= multiplier;
        });
      }
    }

    vitr++;
    count += 2;
  }

  if (vitr != voices.end()) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "decayvoice"_n,
      std::make_tuple(vitr->account.value, chunksize)
    );
  }
}

ACTION referendums::favour (const name & voter, const uint64_t & proposal_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, proposal_id, amount, ProposalsCommon::trust, false);
}

ACTION referendums::against (const name & voter, const uint64_t & proposal_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, proposal_id, amount, ProposalsCommon::distrust, false);
}

ACTION referendums::neutral (const name & voter, const uint64_t & proposal_id) {
  require_auth(voter);
  vote_aux(voter, proposal_id, uint64_t(0), ProposalsCommon::neutral, false);
}

ACTION referendums::revertvote (const name & voter, const uint64_t & proposal_id) {
  require_auth(voter);
  
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

ACTION referendums::voteonbehalf (const name & voter, const uint64_t & proposal_id, const uint64_t & amount, const name & option) {
  require_auth(get_self());
  vote_aux(voter, proposal_id, amount, option, true);
}

ACTION referendums::changetrust (const name & user, const bool & trust) {
  require_auth(get_self());

  voice_tables voice_t(get_self(), campaign_scope.value);
  auto vitr = voice_t.find(user.value);

  if (vitr == voice_t.end() && trust) {
    recover_voice(user);
  } else if (vitr != voice_t.end() && !trust) {
    erase_voice(user);
  }
}

ACTION referendums::delegate (const name & delegator, const name & delegatee, const name & scope) {

  require_auth(delegator);

  voice_tables voice(get_self(), scope.value);
  voice.get(delegator.value, "delegator does not have voice");
  voice.get(delegatee.value, "delegatee does not have voice");

  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto ditr = deltrust_t.find(delegator.value);

  name current = delegatee;
  bool no_cycles = false;
  uint64_t max_depth = config_get("dlegate.dpth"_n);

  for (int i = 0; i < max_depth; i++) {
    auto dditr = deltrust_t.find(current.value);
    if (dditr != deltrust_t.end()) {
      current = dditr -> delegatee;
      if (current == delegator) {
        break;
      }
    } else {
      no_cycles = true;
      break;
    }
  }

  check(no_cycles, "can not add delegatee, cycles are not allowed");

  if (ditr != deltrust_t.end()) {
    deltrust_t.modify(ditr, _self, [&](auto & item){
      item.delegatee = delegatee;
      item.weight = 1.0;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  } else {
    deltrust_t.emplace(_self, [&](auto & item){
      item.delegator = delegator;
      item.delegatee = delegatee;
      item.weight = 1.0;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  }

}

ACTION referendums::undelegate (const name & delegator, const name & scope) {
  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto ditr = deltrust_t.find(delegator.value);

  check(ditr != deltrust_t.end(), "delegator not found");

  if (!has_auth(ditr->delegatee)) {
    require_auth(delegator);
  } else {
    require_auth(ditr->delegatee);
  }

  deltrust_t.erase(ditr);
}

ACTION referendums::mimicvote (
  const name & delegatee, 
  const name & delegator, 
  const name & scope, 
  const uint64_t & proposal_id, 
  const double & percentage_used, 
  const name & option, 
  const uint64_t chunksize) {

  require_auth(get_self());

  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto deltrusts_by_delegatee_delegator = deltrust_t.get_index<"byddelegator"_n>();

  voice_tables voices(get_self(), scope.value);

  uint128_t id = (uint128_t(delegatee.value) << 64) + delegator.value;

  auto ditr = deltrusts_by_delegatee_delegator.lower_bound(id);
  uint64_t count = 0;

  while (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee && count < chunksize) {

    name voter = ditr->delegator;

    auto vitr = voices.find(voter.value);
    if (vitr != voices.end()) {
      send_deferred_transaction(
        permission_level(get_self(), "active"_n),
        get_self(),
        "voteonbehalf"_n,
        std::make_tuple(voter, proposal_id, vitr->balance * percentage_used, option)
      );
    }

    ditr++;
    count++;
  }

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee) {
    send_deferred_transaction(
      permission_level{get_self(), "active"_n},
      get_self(),
      "mimicvote"_n,
      std::make_tuple(delegatee, ditr->delegator, scope, proposal_id, percentage_used, option, chunksize)
    );
  }

}

ACTION referendums::mimicrevert (const name & delegatee, const uint64_t & delegator, const name & scope, const uint64_t & proposal_id, const uint64_t & chunksize) {

  require_auth(get_self());

  proposal_tables proposals_t(get_self(), get_self().value);
  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  check(pitr->stage == ProposalsCommon::stage_active, "proposal stage is not active");

  votes_tables votes_t(get_self(), proposal_id);

  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto deltrusts_by_delegatee_delegator = deltrust_t.get_index<"byddelegator"_n>();

  uint128_t id = (uint128_t(delegatee.value) << 64) + delegator;

  auto ditr = delegator == 0 ? deltrusts_by_delegatee_delegator.lower_bound(id) : deltrusts_by_delegatee_delegator.find(id);

  uint64_t count = 0;

  while (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee && count < chunksize) {

    auto vitr = votes_t.find(ditr->delegator.value);

    if (vitr != votes_t.end()) {
      uint64_t amount = vitr->amount;

      if (vitr->favour == true && amount > 0) {
        votes_t.modify(vitr, _self, [&](auto& item) {
          item.favour = false;
        });

        proposals_t.modify(pitr, _self, [&](auto& proposal) {
          proposal.against += amount;
          proposal.favour -= amount;
        });
      }
    }

    ditr++;
    count++;
  }

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "mimicrevert"_n,
      std::make_tuple(delegatee, ditr->delegator.value, scope, proposal_id, chunksize)
    );
  }

}

ACTION referendums::testsetvoice (const name & account, const uint64_t & amount) {
  require_auth(get_self());
  set_voice(account, amount, "all"_n);
}


void referendums::set_voice (const name & user, const uint64_t & amount, const name & scope) {
  if (scope == "all"_n) {

    bool increase_size = true;

    for (auto & s : scopes) {
      voice_tables voice_t(get_self(), s.value);
      auto vitr = voice_t.find(user.value);

      if (vitr == voice_t.end()) {
        voice_t.emplace(_self, [&](auto & voice){
          voice.account = user;
          voice.balance = amount;
        });
      }
      else {
        increase_size = false;
        voice_t.modify(vitr, _self, [&](auto & voice){
          voice.balance = amount;
        });
      }
    }

    if (increase_size) {
      size_change("voice.sz"_n, 1);
    }

  } else {
    voice_tables voice_t(get_self(), scope.value);
    auto vitr = voice_t.find(user.value);

    if (vitr == voice_t.end()) {
      voice_t.emplace(_self, [&](auto & voice){
        voice.account = user;
        voice.balance = amount;
      });
    } else {
      voice_t.modify(vitr, _self, [&](auto & voice){
        voice.balance = amount;
      });
    }
  }
}

double referendums::voice_change (const name & user, const uint64_t & amount, const bool & reduce, const name & scope) {
  double percentage_used = 0.0;

  if (scope == "all"_n) {

    for (auto & s : scopes) {
      voice_tables voice_t(get_self(), s.value);
      auto vitr = voice_t.find(user.value);

      if (vitr != voice_t.end()) {
        if (reduce) {
          check(amount <= vitr->balance, s.to_string() + " voice balance exceeded");
        }
        voice_t.modify(vitr, _self, [&](auto & voice){
          if (reduce) {
            voice.balance -= amount;
          } else {
            voice.balance += amount;
          }
        });
      }
    }

  } else {    
    voice_tables voice_t(get_self(), scope.value);
    auto vitr = voice_t.require_find(user.value, "user does not have voice");

    if (reduce) {
      check(amount <= vitr->balance, "voice balance exceeded");
      percentage_used = amount / double(vitr -> balance);
    }
    voice_t.modify(vitr, _self, [&](auto & voice){
      if (reduce) {
        voice.balance -= amount;
      } else {
        voice.balance += amount;
      }
    });
  }

  return percentage_used;
}

void referendums::erase_voice (const name & user) {
  require_auth(get_self());

  for (auto & s : scopes) {
    voice_tables voice_t(get_self(), s.value);
    auto vitr = voice_t.find(user.value);
    voice_t.erase(vitr);
  }
  
  size_change("voice.sz"_n, -1);

  active_tables actives_t(get_self(), get_self().value); 
  auto aitr = actives_t.find(user.value);

  if (aitr != actives_t.end()) {
    actives_t.erase(aitr);
    size_change(user_active_size, -1);
  }
}

void referendums::recover_voice (const name & account) {

  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  
  cs_points_tables cspoints_t(contracts::harvest, contracts::harvest.value);

  auto csitr = cspoints_t.find(account.value);
  uint64_t voice_amount = 0;

  if (csitr != cspoints_t.end()) {
    voice_amount = calculate_decay(csitr->rank);
  }

  set_voice(account, voice_amount, "all"_n);
  size_change(cycle_vote_power_size, voice_amount);

}

uint64_t referendums::calculate_decay (const uint64_t & voice_amount) {

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get_or_create(get_self(), cycle_table());
  
  uint64_t decay_percentage = config_get(name("vdecayprntge"));
  uint64_t decay_time = config_get(name("decaytime"));
  uint64_t decay_sec = config_get(name("propdecaysec"));
  uint64_t temp = c.t_onperiod + decay_time;

  check(decay_percentage <= 100, "The decay percentage could not be grater than 100%");

  if (temp >= c.t_voicedecay) { return voice_amount; }
  uint64_t n = ((c.t_voicedecay - temp) / decay_sec) + 1;
  
  double multiplier = 1.0 - (decay_percentage / 100.0);
  return voice_amount * pow(multiplier, n);

}

void referendums::vote_aux (const name & voter, const uint64_t & proposal_id, const uint64_t & amount, const name & option, const bool & is_delegated) {

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
    send_inline_action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(voter, uint64_t(rep * rep_multiplier))
    );
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

bool referendums::has_delegates (const name & voter, const name & scope) {
  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto deltrusts_by_delegatee = deltrust_t.get_index<"bydelegatee"_n>();
  auto ditr = deltrusts_by_delegatee.find(voter.value);
  return ditr != deltrusts_by_delegatee.end();
}

void referendums::add_voted_proposal (const uint64_t & proposal_id) {

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get_or_create(get_self(), cycle_table());

  voted_proposals_tables votedprops_t(get_self(), c.propcycle);
  auto vpitr = votedprops_t.find(proposal_id);

  if (vpitr == votedprops_t.end()) {
    votedprops_t.emplace(_self, [&](auto & prop){
      prop.proposal_id = proposal_id;
    });
  }

}

void referendums::increase_voice_cast (const uint64_t & amount, const name & option, const name & prop_type) {

  if (prop_type == ProposalsCommon::fund_type_none) { return; }

  cycle_tables cycle_t(get_self(), get_self().value);
  cycle_table c = cycle_t.get_or_create(get_self(), cycle_table());

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

void referendums::add_voice_cast (const uint64_t & cycle, const uint64_t & voice_cast, const name & type) {

  support_level_tables support_t(get_self(), type.value);
  auto sitr = support_t.find(cycle);
  check(sitr != support_t.end(), "cycle not found "+std::to_string(cycle));

  support_t.modify(sitr, _self, [&](auto & item){
    uint64_t total_voice = item.total_voice_cast + voice_cast;
    item.total_voice_cast = total_voice;
    item.voice_needed = calc_voice_needed(total_voice, item.num_proposals);
  });

}

uint64_t referendums::calc_voice_needed (const uint64_t & total_voice, const uint64_t & num_proposals) {
  return ceil(total_voice * (get_quorum(num_proposals) / 100.0));
}

uint64_t referendums::get_quorum (const uint64_t & total_proposals) {

  uint64_t base_quorum = config_get("quorum.base"_n);
  uint64_t quorum_min = config_get("quor.min.pct"_n);
  uint64_t quorum_max = config_get("quor.max.pct"_n);

  uint64_t quorum = total_proposals ? base_quorum / total_proposals : 0;
  quorum = std::max(quorum_min, quorum);
  return std::min(quorum_max, quorum);

}


bool referendums::revert_vote (const name & voter, const uint64_t & proposal_id) {

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

bool referendums::is_trust_delegated (const name & account, const name & scope) {
  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto ditr = deltrust_t.find(account.value);
  return ditr != deltrust_t.end();
}

void referendums::send_mimic_delegatee_vote (const name & delegatee, const name & scope, const uint64_t & proposal_id, const double & percentage_used, const name & option) {

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

uint64_t referendums::active_cutoff_date () {
  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t prop_cycle_sec = config_get(name("propcyclesec"));
  uint64_t inact_cycles = config_get(name("inact.cyc"));
  return now - (inact_cycles * prop_cycle_sec);
}

void referendums::check_citizen (const name & account) {
  DEFINE_USER_TABLE;
  DEFINE_USER_TABLE_MULTI_INDEX;
  user_tables users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(account.value);
  check(uitr != users.end(), "user not found");
  check(uitr->status == name("citizen"), "user is not a citizen");
}

void referendums::check_attributes (const std::map<std::string, VariantValue> & args) {

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
void referendums::send_inline_action (
  const permission_level & permission, 
  const name & contract, 
  const name & action, 
  const std::tuple<T...> & data) {

  eosio::action(permission, contract, action, data).send();

}

template <typename... T>
void referendums::send_deferred_transaction (
  const permission_level & permission,
  const name & contract, 
  const name & action,  
  const std::tuple<T...> & data) {

  print("calling deferred transaction!\n");

  // eosio::action(
  //   permission,
  //   contract,
  //   action,
  //   data
  // ).send();

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

