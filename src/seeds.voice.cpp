#include <seeds.voice.hpp>

ACTION voice::reset () {
  require_auth(get_self());

  for (auto & s : scopes) {
    voice_tables voice_t(get_self(), s.value);
    auto vitr = voice_t.begin();
    while (vitr != voice_t.end()) {
      vitr = voice_t.erase(vitr);
    }
  }

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }
}

ACTION voice::setvoice (const name & user, const uint64_t & amount, const uint64_t & scope) {
  require_auth(get_self());
  set_voice(user, amount, scope);
}

ACTION voice::addvoice (const name & user, const uint64_t & amount, const uint64_t & scope) {
  require_auth(get_self());
  voice_change(user, amount, false, scope);
}

ACTION voice::subvoice (const name & user, const uint64_t & amount, const uint64_t & scope) {
  require_auth(get_self());
  voice_change(user, amount, true, scope);
}

ACTION voice::changetrust(name user, bool trust) {
  require_auth(get_self());

  voice_tables voice_t(get_self(), scopes[0].value);
  auto vitr = voice_t.find(user.value);

  if (vitr == voice_t.end() && trust) {
    recover_voice(user);
  } else if (vitr != voice.end() && !trust) {
    erase_voice(user);
  }
}

ACTION voice::vote (const name & voter, const uint64_t & amount, const name & scope) {
  require_auth(get_self());

  double percentage_used = voice_change(voter, amount, true, scope);

  // potentially all the logic regarded to participants, actives and voice delegation could go here
}

ACTION voice::decayvoices() {
  require_auth(get_self());

  cycle_tables cycle_t(contracts::proposals, contracts::proposals.value);
  cycle_table c = cycle_t.get();

  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t decay_time = config_get(name("decaytime"));
  uint64_t decay_sec = config_get(name("propdecaysec"));

  if ((c.t_onperiod < now)
      && (now - c.t_onperiod >= decay_time)
      && (now - c.t_voicedecay >= decay_sec)
  ) {
    c.t_voicedecay = now;
    cycle.set(c, get_self());
    uint64_t batch_size = config_get(name("batchsize"));
    decayvoice(0, batch_size);
  }
}

ACTION voice::decayvoice(const uint64_t & start, const uint64_t & chunksize) {
  require_auth(get_self());

  uint64_t percentage_decay = config_get(name("vdecayprntge"));
  check(percentage_decay <= 100, "Voice decay parameter can not be more than 100%.");
  
  auto vitr = start == 0 ? voice.begin() : voice.find(start);
  uint64_t count = 0;

  double multiplier = (100.0 - (double)percentage_decay) / 100.0;

  while (vitr != voice.end() && count < chunksize) {
    auto vaitr = voice_alliance.find(vitr->account.value);
    auto hvitr = voice_milestone.find(vitr->account.value);

    voice.modify(vitr, _self, [&](auto & v){
      v.balance *= multiplier;
    });

    if (vaitr != voice_alliance.end()) {
      voice_alliance.modify(vaitr, _self, [&](auto & va){
        va.balance *= multiplier;
      });
    }

    if (hvitr != voice_milestone.end()) {
      voice_milestone.modify(hvitr, _self, [&](auto & hv){
        hv.balance *= multiplier;
      });
    }
    vitr++;
    count++;
  }

  if (vitr != voice.end()) {
    uint64_t next_value = vitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "decayvoice"_n,
        std::make_tuple(next_value, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

ACTION voice::updatevoices() {
  require_auth(get_self());
  updatevoice((uint64_t)0);
}

ACTION voice::updatevoice(uint64_t start) {
  require_auth(get_self());
  
  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  
  uint64_t cutoff_date = active_cutoff_date();

  cs_points_tables cspoints(contracts::harvest, contracts::harvest.value);
  voice_tables voice_alliance(get_self(), alliance_type.value);

  auto vitr = start == 0 ? voice.begin() : voice.find(start);

  if (start == 0) {
      size_set(cycle_vote_power_size, 0);
      size_set(user_active_size, 0);
  }

  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t count = 0;
  uint64_t vote_power = 0;
  uint64_t active_users = 0;
  
  while (vitr != voice.end() && count < batch_size) {
      auto csitr = cspoints.find(vitr->account.value);
      uint64_t points = 0;
      if (csitr != cspoints.end()) {
        points = csitr -> rank;
      }

      set_voice(vitr -> account, points, "all"_n);

      if (is_active(vitr -> account, cutoff_date)) {
        vote_power += points;
        active_users++;
      }

      vitr++;
      count++;
  }
  
  size_change(cycle_vote_power_size, vote_power);
  size_change(user_active_size, active_users);

  if (vitr != voice.end()) {
    uint64_t next_value = vitr->account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "updatevoice"_n,
        std::make_tuple(next_value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void voice::set_voice (const name & user, const uint64_t & amount, const name & scope) {
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

double voice::voice_change (const name & user, const uint64_t & amount, const bool & reduce, const name & scope) {
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

void voice::erase_voice (const name & user) {
  require_auth(get_self());

  for (auto & s : scopes) {
    voice_tables voice_t(get_self(), s.value);
    auto vitr = voice_t.find(user.value);
    voice_t.erase(vitr);
  }
  
  size_change("voice.sz"_n, -1);
  
  // auto aitr = actives.find(user.value);
  // if (aitr != actives.end()) {
  //   actives.erase(aitr);
  //   size_change(user_active_size, -1);
  // }
}

void proposals::recover_voice(const name & account) {

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

uint64_t voice::calculate_decay(const uint64_t & voice_amount) {

  cycle_tables cycle_t(contracts::proposals, contracts::proposals.value);
  cycle_table c = cycle_t.get();
  
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

