#include <seeds.voice.hpp>

ACTION voice::reset () {
  require_auth(get_self());

  voice_tables voice_t(get_self(), get_self().value);
  auto vitr = voice_t.begin();
  while (vitr != voice_t.end()) {
    vitr = voice_t.erase(vitr);
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

ACTION voice::changetrust(name user, bool trust) {
  require_auth(get_self());

  // commented due to voice decay not implemented in this contract yet

  // auto vitr = voice.find(user.value);

  // if (vitr == voice.end() && trust) {
  //   recover_voice(user); // Issue 
  //   //set_voice(user, 0, ""_n);
  // } else if (vitr != voice.end() && !trust) {
  //   erase_voice(user);
  // }
}

ACTION voice::vote (const name & voter, const uint64_t & amount, const name & scope) {
  require_auth(get_self());

  double percentage_used = voice_change(voter, amount, true, scope);

  // potentially all the logic regarded to participants, actives and voice delegation could go here
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

