#include <seeds.dao.hpp>


ACTION dao::updatevoices () {
  require_auth(get_self());
  updatevoice(uint64_t(0), ""_n);
}

ACTION dao::updatevoice (const uint64_t & start, const name & scope) {
  require_auth(get_self());
  
  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  
  uint64_t cutoff_date = active_cutoff_date();
  cs_points_tables cspoints_t(contracts::harvest, contracts::harvest.value);
  voice_tables voices_t(get_self(), campaign_scope.value);
  auto vitr = start == 0 ? voices_t.begin() : voices_t.find(start);
  if (start == 0) {
      size_set(user_active_size, 0);
  }
  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;
  uint64_t vote_power = 0;
  uint64_t active_users = 0;
  
  while (vitr != voices_t.end() && count < batch_size) {
      auto csitr = cspoints_t.find(vitr->account.value);
      uint64_t points = 0;
      if (csitr != cspoints_t.end()) {
        points = csitr->rank;
      }
      print("account: ", vitr->account, ", points: ", points, scope);
      set_voice(vitr->account, points, scope);
      if (is_active(vitr -> account, cutoff_date)) {
        vote_power += points;
        active_users++;
      }
      vitr++;
      count++;
  }
  
  size_change(user_active_size, active_users);
  if (vitr != voices_t.end()) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "updatevoice"_n,
      std::make_tuple(vitr->account.value, scope)
    );
  }
}


ACTION dao::deletescope (const uint64_t & start, const name & scope) {

  require_auth(get_self());

  bool existing_scope = false;

  for (auto & s : scopes) {
    if (s == scope) {
      existing_scope = true;
    }
  }
  
  check(existing_scope, "scope must exist");
  
  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;

  voice_tables voice_t(get_self(), scope.value);
  auto vitr = start == 0 ? voice_t.begin() : voice_t.find(start);
  
  while (vitr != voice_t.end() && count < batch_size) {
    vitr = voice_t.erase(vitr);
    count++;
  }
  
  if (vitr != voice_t.end()) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "deletescope"_n,
      std::make_tuple(vitr->account.value, scope)
    );
  }
}

ACTION dao::addvoice(const uint64_t & start, const name & scope) {

  require_auth(get_self());

  bool existing_scope = false;

  for (auto & s : scopes) {
    if (s == scope) { existing_scope = true; }
  }

  check(existing_scope, "scope must exist");

  cs_points_tables cspoints_t(contracts::harvest, contracts::harvest.value);

  voice_tables voices_t(get_self(), campaign_scope.value);
  auto vitr = start == 0 ? voices_t.begin() : voices_t.find(start);

  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;

  while (vitr != voices_t.end() && count < batch_size) {
    auto csitr = cspoints_t.find(vitr->account.value);
    uint64_t voice_amount = 0;

    if (csitr != cspoints_t.end()) {
      voice_amount = calculate_decay(csitr->rank);
    }

    print("account: ", vitr->account, ", points: ", voice_amount, scope);
    set_voice(vitr->account, voice_amount, scope);

    vitr++;
    count++;
  }

  if (vitr != voices_t.end()) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "addvoice"_n,
      std::make_tuple(vitr->account.value, scope)
    );
  }


}


ACTION dao::decayvoices () {
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

ACTION dao::decayvoice (const uint64_t & start, const uint64_t & chunksize) {
  require_auth(get_self());

  voice_tables voices(get_self(), campaign_scope.value);

  print("decaying voices!\n", "chunksize:", chunksize, "\n");

  uint64_t percentage_decay = config_get(name("vdecayprntge"));
  check(percentage_decay <= 100, "Voice decay parameter can not be more than 100%.");
  
  auto vitr = voices.lower_bound(start);
  uint64_t count = 0;

  double multiplier = (100.0 - (double)percentage_decay) / 100.0;

  while (vitr != voices.end() && count < chunksize) {

    print("account:", vitr->account, "\n");

    for (auto & s : scopes) {
      voice_tables voice_t(get_self(), s.value);
      auto voice_itr = voice_t.find(vitr->account.value);

      if (voice_itr != voice_t.end()) {
        voice_t.modify(voice_itr, _self, [&](auto & v){
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



ACTION dao::voteonbehalf (const name & voter, const uint64_t & proposal_id, const uint64_t & amount, const name & option) {
  require_auth(get_self());
  vote_aux(voter, proposal_id, amount, option, true);
}

ACTION dao::changetrust (const name & user, const bool & trust) {
  require_auth(get_self());

  voice_tables voice_t(get_self(), campaign_scope.value);
  auto vitr = voice_t.find(user.value);

  if (vitr == voice_t.end() && trust) {
    recover_voice(user);
  } else if (vitr != voice_t.end() && !trust) {
    erase_voice(user);
  }
}

ACTION dao::delegate (const name & delegator, const name & delegatee, const name & scope) {

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

ACTION dao::undelegate (const name & delegator, const name & scope) {
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

ACTION dao::mimicvote (
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
        std::make_tuple(voter, proposal_id, uint64_t(vitr->balance * percentage_used), option)
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

ACTION dao::mimicrevert (const name & delegatee, const uint64_t & delegator, const name & scope, const uint64_t & proposal_id, const uint64_t & chunksize) {

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

      send_deferred_transaction(
        permission_level(get_self(), "active"_n),
        get_self(),
        "revertvote"_n,
        std::make_tuple(ditr->delegator, proposal_id)
      );
      
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

ACTION dao::dhomimicvote (const name & delegatee, const uint64_t & start, std::vector<DhoVote> votes, const uint64_t & chunksize) {

  require_auth(get_self());

  delegate_trust_tables deltrust_t(get_self(), dhos_scope.value);
  auto deltrusts_by_delegatee_delegator = deltrust_t.get_index<"byddelegator"_n>();

  auto ditr = deltrusts_by_delegatee_delegator.lower_bound((uint128_t(delegatee.value) << 64) + start);
  uint64_t count = 0;

  while (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee && count < chunksize) {

    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "votedhos"_n,
      std::make_tuple(ditr->delegator, votes)
    );

    ditr++;
    count+=2;
  }

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr->delegatee == delegatee) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "dhomimicvote"_n,
      std::make_tuple(delegatee, ditr->delegator.value, votes, chunksize)
    );
  }

}

ACTION dao::testsetvoice (const name & account, const uint64_t & amount) {
  require_auth(get_self());
  set_voice(account, amount, "all"_n);
}


void dao::set_voice (const name & user, const uint64_t & amount, const name & scope) {
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

double dao::voice_change (const name & user, const uint64_t & amount, const bool & reduce, const name & scope) {
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

void dao::erase_voice (const name & user) {
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

void dao::recover_voice (const name & account) {

  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  
  cs_points_tables cspoints_t(contracts::harvest, contracts::harvest.value);

  auto csitr = cspoints_t.find(account.value);
  uint64_t voice_amount = 0;

  if (csitr != cspoints_t.end()) {
    voice_amount = calculate_decay(csitr->rank);
  }

  set_voice(account, voice_amount, "all"_n);

}

uint64_t dao::calculate_decay (const uint64_t & voice_amount) {

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

bool dao::has_delegates (const name & voter, const name & scope) {
  delegate_trust_tables deltrust_t(get_self(), scope.value);
  auto deltrusts_by_delegatee = deltrust_t.get_index<"bydelegatee"_n>();
  auto ditr = deltrusts_by_delegatee.find(voter.value);
  return ditr != deltrusts_by_delegatee.end();
}

bool dao::is_active (const name & account, const uint64_t & cutoff_date) {
  active_tables actives_t(get_self(), get_self().value);
  auto aitr = actives_t.find(account.value);
  return aitr != actives_t.end() && aitr->timestamp > cutoff_date;
}

