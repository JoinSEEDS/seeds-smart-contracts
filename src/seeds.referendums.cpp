#include <seeds.referendums.hpp>

void referendums::send_change_setting(name setting_name, uint64_t setting_value) {
  action(
    permission_level{contracts::settings, "active"_n},
    contracts::settings, "configure"_n,
    make_tuple(setting_name, setting_value)
  ).send();
}

void referendums::send_burn_stake(asset quantity) {
  action(
    permission_level{contracts::referendums, "active"_n},
    contracts::token, "burn"_n,
    make_tuple(contracts::referendums, quantity)
  ).send();
}

void referendums::send_refund_stake(name account, asset quantity) {
  action(
    permission_level{contracts::referendums, "active"_n},
    contracts::token, "transfer"_n,
    make_tuple(contracts::referendums, account, quantity, string(""))
  ).send();
}

void referendums::send_onperiod() {
  action(
    permission_level{contracts::referendums, "active"_n},
    get_self(), "onperiod"_n,
    make_tuple()
  ).send();
}

void referendums::give_voice() {
  auto bitr = balances.begin();

  while (bitr != balances.end()) {
    balances.modify(bitr, get_self(), [&](auto& item) {
      item.voice = 10;
    });

    bitr++;
  }
}

void referendums::run_testing() {
  referendum_tables testing(get_self(), name("testing").value);
  referendum_tables passed(get_self(), name("passed").value);
  referendum_tables failed(get_self(), name("failed").value);

  auto titr = testing.begin();

  uint64_t majority = config.find(name("refsmajority").value)->value;

  while (titr != testing.end()) {
    bool referendum_passed = utils::is_valid_majority(titr->favour, titr->against, majority);

    if (referendum_passed) {
      send_change_setting(titr->setting_name, titr->setting_value);

      passed.emplace(_self, [&](auto& item) {
        MOVE_REFERENDUM(titr, item)
      });

      titr = testing.erase(titr);
    } else {
      failed.emplace(_self, [&](auto& item) {
        MOVE_REFERENDUM(titr, item)
      });

      titr = testing.erase(titr);
    }
  }
}

void referendums::run_active() {
  referendum_tables active(get_self(), name("active").value);
  referendum_tables testing(get_self(), name("testing").value);
  referendum_tables failed(get_self(), name("failed").value);

  auto aitr = active.begin();

  uint64_t majority = config.find(name("refsmajority").value)->value;
  uint64_t quorum = config.find(name("refsquorum").value)->value;

  while (aitr != active.end()) {
    voter_tables voters(get_self(), aitr->referendum_id);
    uint64_t voters_number = distance(voters.begin(), voters.end());
    
    bool valid_majority = utils::is_valid_majority(aitr->favour, aitr->against, majority);
    bool valid_quorum = utils::is_valid_quorum(voters_number, quorum, distance(balances.begin(), balances.end()));
    bool referendum_passed = valid_majority && valid_quorum;

    if (referendum_passed) {
      testing.emplace(_self, [&](auto& item) {
        MOVE_REFERENDUM(aitr, item)
      });

      send_refund_stake(aitr->creator, aitr->staked);

      aitr = active.erase(aitr);
    } else {
      send_burn_stake(aitr->staked);

      failed.emplace(_self, [&](auto& item) {
        MOVE_REFERENDUM(aitr, item)
      });

      aitr = active.erase(aitr);
    }
  }
}

void referendums::run_staged() {
  referendum_tables staged(get_self(), name("staged").value);
  referendum_tables active(get_self(), name("active").value);

  auto sitr = staged.begin();

  while (sitr != staged.end()) {
    active.emplace(_self, [&](auto& item) {
      MOVE_REFERENDUM(sitr, item)
    });

    sitr = staged.erase(sitr);
  }
}

void referendums::onperiod() {
  run_testing();
  run_active();
  run_staged();

  give_voice();

}

void referendums::reset() {
  auto bitr = balances.begin();

  while (bitr != balances.end()) {
    bitr = balances.erase(bitr);
  }

  referendum_tables staged(get_self(), name("staged").value);
  referendum_tables active(get_self(), name("active").value);
  referendum_tables testing(get_self(), name("testing").value);
  referendum_tables passed(get_self(), name("passed").value);
  referendum_tables failed(get_self(), name("failed").value);

  auto sitr = staged.begin();
  auto aitr = active.begin();
  auto titr = testing.begin();
  auto pitr = passed.begin();
  auto fitr = failed.begin();

  while (sitr != staged.end()) {
    voter_tables voters(get_self(), sitr->referendum_id);
    auto vitr = voters.begin();
    while (vitr != voters.end()) {
      vitr = voters.erase(vitr);
    }

    sitr = staged.erase(sitr);
  }

  while (aitr != active.end()) {
    voter_tables voters(get_self(), aitr->referendum_id);
    auto vitr = voters.begin();
    while (vitr != voters.end()) {
      vitr = voters.erase(vitr);
    }

    aitr = active.erase(aitr);
  }

  while (titr != testing.end()) {
    voter_tables voters(get_self(), titr->referendum_id);
    auto vitr = voters.begin();
    while (vitr != voters.end()) {
      vitr = voters.erase(vitr);
    }

    titr = testing.erase(titr);
  }

  while (pitr != passed.end()) {
    voter_tables voters(get_self(), pitr->referendum_id);
    auto vitr = voters.begin();
    while (vitr != voters.end()) {
      vitr = voters.erase(vitr);
    }

    pitr = passed.erase(pitr);
  }

  while (fitr != failed.end()) {
    voter_tables voters(get_self(), fitr->referendum_id);
    auto vitr = voters.begin();
    while (vitr != voters.end()) {
      vitr = voters.erase(vitr);
    }

    fitr = failed.erase(fitr);
  }
}

void referendums::addvoice(name account, uint64_t amount) {
  auto bitr = balances.find(account.value);

  if (bitr == balances.end()) {
    balances.emplace(get_self(), [&](auto& balance) {
      balance.account = account;
      balance.voice = amount;
      balance.stake = asset(0, seeds_symbol);
    });
  } else {
    balances.modify(bitr, get_self(), [&](auto& balance) {
      balance.voice += amount;
    });
  }
}

void referendums::stake(name from, name to, asset quantity, string memo) {
  if (to == get_self()) {
    utils::check_asset(quantity);

    auto bitr = balances.find(from.value);

    if (bitr == balances.end()) {
      balances.emplace(get_self(), [&](auto& balance) {
        balance.account = from;
        balance.voice = 0;
        balance.stake = quantity;
      });
    } else {
      balances.modify(bitr, get_self(), [&](auto& balance) {
        balance.stake += quantity;
      });
    }
  }
}

void referendums::create(
  name creator,
  name setting_name,
  uint64_t setting_value,
  string title,
  string summary,
  string description,
  string image,
  string url
) {
  require_auth(creator);

  uint64_t price_amount = config.find(name("refsnewprice").value)->value;
  asset stake_price = asset(price_amount, seeds_symbol);

  auto bitr = balances.find(creator.value);
  check(bitr != balances.end(), "user has not balance");
  check(bitr->stake >= stake_price, "user has not sufficient stake");

  referendum_tables staged(get_self(), name("staged").value);

  staged.emplace(get_self(), [&](auto& item) {
    item.referendum_id = staged.available_primary_key();
    item.favour = 0;
    item.against = 0;
    item.staked = asset(price_amount, seeds_symbol);
    item.creator = creator;
    item.setting_name = setting_name;
    item.setting_value = setting_value;
    item.title = title;
    item.summary = summary;
    item.description = description;
    item.image = image;
    item.url = url;
    item.created_at = current_time_point().sec_since_epoch();
  });

  balances.modify(bitr, get_self(), [&](auto& balance) {
    balance.stake -= stake_price;
  });
}

void referendums::update(
  name creator,
  name setting_name,
  uint64_t setting_value,
  string title,
  string summary,
  string description,
  string image,
  string url
) {
  referendum_tables staged(get_self(), name("staged").value);
  auto refs = staged.get_index<"byname"_n>();
  auto sitr = refs.find(setting_name.value);
  
  check (sitr != refs.end(), "referendum setting not found " + setting_name.to_string());

  while (sitr != refs.end()) {
    if (sitr->creator == creator) {
      break;
    }
    sitr++;
  }
  
  check (sitr != refs.end(), "referendum creator not found " + creator.to_string());

  require_auth(sitr->creator);

  refs.modify(sitr, get_self(), [&](auto& item) {
    item.setting_name = setting_name;
    item.setting_value = setting_value;
    item.title = title;
    item.summary = summary;
    item.description = description;
    item.image = image;
    item.url = url;
  });
}

void referendums::favour(name voter, uint64_t referendum_id, uint64_t amount) {
  auto bitr = balances.find(voter.value);
  check(bitr != balances.end(), "user has no voice");
  check(bitr->voice >= amount, "not enough voice");

  voter_tables voters(get_self(), referendum_id);
  auto vitr = voters.find(voter.value);
  check(vitr == voters.end(), "user can only vote one time");

  referendum_tables active(get_self(), name("active").value);
  auto aitr = active.find(referendum_id);
  check (aitr != active.end(), "referendum does not exist");

  balances.modify(bitr, get_self(), [&](auto& balance) {
    balance.voice -= amount;
  });

  active.modify(aitr, get_self(), [&](auto& referendum) {
    referendum.favour += amount;
  });

  voters.emplace(get_self(), [&](auto& item) {
    item.account = voter;
    item.referendum_id = referendum_id;
    item.amount = amount;
    item.favoured = true;
    item.canceled = false;
  });
}

void referendums::against(name voter, uint64_t referendum_id, uint64_t amount) {
  require_auth(voter);

  auto bitr = balances.find(voter.value);
  check(bitr != balances.end(), "user has no voice");
  check(bitr->voice >= amount, "not enough voice");

  referendum_tables active(get_self(), name("active").value);
  auto aitr = active.find(referendum_id);
  check(aitr != active.end(), "referendum not found");

  voter_tables voters(get_self(), referendum_id);
  auto vitr = voters.find(voter.value);
  check(vitr == voters.end(), "user can only vote one time");

  balances.modify(bitr, get_self(), [&](auto& balance) {
    balance.voice -= amount;
  });

  active.modify(aitr, get_self(), [&](auto& item) {
    item.against += amount;
  });

  voters.emplace(get_self(), [&](auto& item) {
    item.account = voter;
    item.referendum_id = referendum_id;
    item.amount = amount;
    item.favoured = false;
    item.canceled = false;
  });
}

void referendums::cancelvote(name voter, uint64_t referendum_id) {
  require_auth(voter);

  voter_tables voters(get_self(), referendum_id);
  auto vitr = voters.find(voter.value);
  check(vitr != voters.end(), "vote not found");
  check(vitr->canceled == false, "vote already canceled");

  referendum_tables testing(get_self(), name("testing").value);
  auto titr = testing.find(referendum_id);
  check(titr != testing.end(), "testing referendum not found");

  voters.modify(vitr, get_self(), [&](auto& voter) {
    voter.canceled = true;
  });

  if (vitr->favoured == true) {
    testing.modify(titr, get_self(), [&](auto& item) {
      item.favour -= vitr->amount;
    });
  } else {
    testing.modify(titr, get_self(), [&](auto& item) {
      item.against -= vitr->amount;
    });
  }
}
