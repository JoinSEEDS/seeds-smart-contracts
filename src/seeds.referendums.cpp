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

uint64_t referendums::get_quorum(const name & setting) {
  auto citr = config.find(setting.value);
  if (citr == config.end()) {
    return config.find(name("quorum.high").value) -> value;
  }
  switch (citr->impact) {
    case high_impact:
      return config.find(name("quorum.high").value) -> value;
    case medium_impact:
      return config.find(name("quorum.med").value) -> value;
    case low_impact:
      return config.find(name("quorum.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      break;
  }
}

uint64_t referendums::get_unity(const name & setting) {
  auto citr = config.find(setting.value);
  if (citr == config.end()) {
    return config.find(name("unity.high").value) -> value;
  }
  switch (citr->impact) {
    case high_impact:
      return config.find(name("unity.high").value) -> value;
    case medium_impact:
      return config.find(name("unity.medium").value) -> value;
    case low_impact:
      return config.find(name("unity.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      break;
  }
}

void referendums::run_testing() {
  referendum_tables testing(get_self(), name("testing").value);
  referendum_tables passed(get_self(), name("passed").value);
  referendum_tables failed(get_self(), name("failed").value);

  auto titr = testing.begin();

  uint64_t majority = 0;

  while (titr != testing.end()) {
    majority = get_unity(titr->setting_name);

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

  uint64_t majority = 0;
  uint64_t quorum = 0;

  while (aitr != active.end()) {
    majority = get_unity(aitr->setting_name);
    quorum = get_quorum(aitr->setting_name);

    voter_tables voters(get_self(), aitr->referendum_id);
    uint64_t voters_number = distance(voters.begin(), voters.end());
    uint64_t citizens_number = distance(balances.begin(), balances.end());
    
    bool valid_majority = utils::is_valid_majority(aitr->favour, aitr->against, majority);
    bool valid_quorum = utils::is_valid_quorum(voters_number, quorum, citizens_number);

// useful for debugging why a proposal failed
// print(" eval " + std::to_string(aitr->referendum_id));
// print(" valid_majority " + std::to_string(valid_majority));
// print(" valid_quorum " + std::to_string(valid_quorum));
// print(" voters_number " + std::to_string(voters_number));
// print(" quorum " + std::to_string(quorum));
// print(" citizens_number " + std::to_string(citizens_number));

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

  require_auth(get_self());

  run_testing();
  run_active();
  run_staged();

  updatevoice(0, 50);

}

void referendums::refundstake(name sponsor) {

  require_auth(sponsor);

  auto bitr = balances.find(sponsor.value);
  check(bitr != balances.end(), "user has no balance");
  check(bitr->stake.amount > 0, "user has no balance");

  asset quantity = bitr->stake; 

  balances.modify(bitr, get_self(), [&](auto& balance) {
    balance.stake -= quantity;
  });

  send_refund_stake(sponsor, quantity);

}

void referendums::reset() {

  require_auth(get_self());

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
  require_auth(get_self());

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

ACTION referendums::updatevoice(uint64_t start, uint64_t batchsize) {
  require_auth(get_self());
  
  // Voice table definition from proposals.hpp
  TABLE voice_table {
    name account;
    uint64_t balance;
    uint64_t primary_key()const { return account.value; }
  };
  typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;
  voice_tables voice(contracts::proposals, "referendum"_n.value);

  auto vitr = start == 0 ? voice.begin() : voice.find(start);

  uint64_t count = 0;
  
  while (vitr != voice.end() && count < batchsize) {
    auto bitr = balances.find(vitr->account.value);
    if (bitr == balances.end()) {
      balances.emplace(get_self(), [&](auto& item) {
        item.account = vitr->account;
        item.voice = vitr->balance;
        item.stake = asset(0, seeds_symbol);
      });
      count++;// double weight for emplace
    } else {
      balances.modify(bitr, get_self(), [&](auto& item) {
        item.voice = vitr->balance;
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
        "updatevoice"_n,
        std::make_tuple(next_value, batchsize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void referendums::stake(name from, name to, asset quantity, string memo) {
  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
        to  ==  get_self() &&                     // to here
        quantity.symbol == seeds_symbol) {        // SEEDS symbol

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

  check_citizen(creator);

  check_values(title, summary, description, image, url);

  uint64_t price_amount = config.find(name("refsnewprice").value)->value;
  asset stake_price = asset(price_amount, seeds_symbol);

  auto bitr = balances.find(creator.value);
  check(bitr != balances.end(), "user has not balance");
  check(bitr->stake >= stake_price, "user has not sufficient stake");

  referendum_tables staged(get_self(), name("staged").value);
  referendum_tables active(get_self(), name("active").value);
  referendum_tables testing(get_self(), name("testing").value);
  referendum_tables passed(get_self(), name("passed").value);
  referendum_tables failed(get_self(), name("failed").value);

  uint64_t key = std::max(staged.available_primary_key(), active.available_primary_key());
  key = std::max(key, testing.available_primary_key());
  key = std::max(key, passed.available_primary_key());
  key = std::max(key, failed.available_primary_key());

  staged.emplace(get_self(), [&](auto& item) {
    item.referendum_id = key;
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

void referendums::check_values(
  string title,
  string summary,
  string description,
  string image,
  string url
) {
  // Title
  check(title.size() <= 128, "title must be less or equal to 128 characters long");
  check(title.size() > 0, "must have a title");

  // Summary
  check(summary.size() <= 1024, "summary must be less or equal to 1024 characters long");

  // Description
  check(description.size() > 0, "must have description");

  // Image
  check(image.size() <= 512, "image url must be less or equal to 512 characters long");
  check(image.size() > 0, "must have image");

  // URL
  check(url.size() <= 512, "url must be less or equal to 512 characters long");
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

  check_values(title, summary, description, image, url);

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
  require_auth(voter);

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

void referendums::check_citizen(name account)
{
  DEFINE_USER_TABLE;
  DEFINE_USER_TABLE_MULTI_INDEX;
  user_tables users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
  check(uitr->status == name("citizen"), "user is not a citizen");
}


void referendums::cancel(uint64_t id) {
  
  referendum_tables staged(get_self(), name("staged").value);

  auto sitr = staged.find(id);
  check(sitr != staged.end(), "Referendum is not staged or does not exist.");

  require_auth(sitr->creator);
  
  send_refund_stake(sitr->creator, sitr->staked);

  staged.erase(sitr);

}
