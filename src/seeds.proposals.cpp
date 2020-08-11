#include <seeds.proposals.hpp>
#include <eosio/system.hpp>

void proposals::reset() {
  require_auth(_self);

  auto pitr = props.begin();

  while (pitr != props.end()) {
    votes_tables votes(get_self(), pitr->id);

    auto voteitr = votes.begin();
    while (voteitr != votes.end()) {
      voteitr = votes.erase(voteitr);
    }

    pitr = props.erase(pitr);
  }

  auto voiceitr = voice.begin();
  while (voiceitr != voice.end()) {
    voiceitr = voice.erase(voiceitr);
  }

  auto paitr = participants.begin();
  while (paitr != participants.end()) {
    paitr = participants.erase(paitr);
  }
}

void proposals::onperiod() {
    require_auth(_self);

    auto pitr = props.begin();

    auto min_stake_param = config.get(name("propminstake").value, "The propminstake parameter has not been initialized yet.");
    
    auto prop_majority = config.get(name("propmajority").value, "The propmajority parameter has not been initialized yet.");
    uint64_t quorum = config.find(name("propquorum").value)->value;

    uint64_t number_active_proposals = 0;
    uint64_t min_stake = min_stake_param.value;
    uint64_t total_eligible_voters = distance(voice.begin(), voice.end());

    while (pitr != props.end()) {
      if (pitr->stage == name("active")) {
        number_active_proposals += 1;
        votes_tables votes(get_self(), pitr->id);
        uint64_t voters_number = distance(votes.begin(), votes.end());

        double majority = double(prop_majority.value) / 100.0;
        double fav = double(pitr->favour);
        bool passed = pitr->favour > 0 && fav >= double(pitr->favour + pitr->against) * majority;
        bool valid_quorum = utils::is_valid_quorum(voters_number, quorum, total_eligible_voters);
        bool is_alliance_type = pitr->fund == bankaccts::alliances;
        bool is_campaign_type = pitr->fund == bankaccts::campaigns;

        if (passed && valid_quorum) {
            if (pitr->staked >= asset(min_stake, seeds_symbol)) {

              refund_staked(pitr->creator, pitr->staked);

              change_rep(pitr->creator, true);

              if (is_alliance_type) {
                send_to_escrow(pitr->fund, pitr->recipient, pitr->quantity, "proposal id: "+std::to_string(pitr->id));
              } else {
                withdraw(pitr->recipient, pitr->quantity, pitr->fund, "");// TODO limit by amount available
              }

              props.modify(pitr, _self, [&](auto& proposal) {
                  proposal.executed = true;
                  proposal.staked = asset(0, seeds_symbol);
                  proposal.status = name("passed");
                  proposal.stage = name("done");
              });
            }
        } else {
          burn(pitr->staked);

          props.modify(pitr, _self, [&](auto& proposal) {
              proposal.executed = false;
              proposal.staked = asset(0, seeds_symbol);
              proposal.status = name("rejected");
              proposal.stage = name("done");
          });
        }
      }

      if (pitr->stage == name("staged")) {
        props.modify(pitr, _self, [&](auto& proposal) {
          proposal.stage = name("active");
        });
      }

      pitr++;
    }

    update_voice_table();
    
    transaction trx_erase_participants{};
    trx_erase_participants.actions.emplace_back(
      permission_level(_self, "active"_n),
      _self,
      "erasepartpts"_n,
      std::make_tuple(number_active_proposals)
    );
    // I don't know how long delay I should use
    // trx_erase_participants.delay_sec = 5;
    trx_erase_participants.send(eosio::current_time_point().sec_since_epoch(), _self);

    // transaction trx{};
    // trx.actions.emplace_back(
    //   permission_level(_self, "active"_n),
    //   _self,
    //   "onperiod"_n,
    //   std::make_tuple()
    // );
    // trx.delay_sec = get_cycle_period_sec(); 
    // trx.send(eosio::current_time_point().sec_since_epoch(), _self);

    update_cycle();
}

void proposals::update_voice_table() {
  
  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  //eosio::multi_index<"cspoints"_n, harvest_table> harveststat(contracts::harvest, contracts::harvest.value);
  
  cs_points_tables cspoints(contracts::harvest, contracts::harvest.value);

  auto vitr = voice.begin();
  while (vitr != voice.end()) {
      auto csitr = cspoints.find(vitr->account.value);
      if (csitr != cspoints.end()) {
        voice.modify(vitr, _self, [&](auto& item) {
          item.balance = csitr->rank;
        });
      } else {
        voice.modify(vitr, _self, [&](auto& item) {
          item.balance = 0;
        });

      }
      vitr++;
  }
}

uint64_t proposals::get_cycle_period_sec() {
  auto moon_cycle = config.get(name("mooncyclesec").value, "The mooncyclesec parameter has not been initialized yet.");
  return moon_cycle.value / 2; // Using half moon cycles for now
}

uint64_t proposals::get_voice_decay_period_sec() {
  auto voice_decay_period = config.get(name("propdecaysec").value, "The propdecaysec parameter has not been initialized yet.");
  return voice_decay_period.value;
}

void proposals::decayvoice() {
  // Not yet implemented    
  require_auth(get_self());

}

void proposals::update_cycle() {
    cycle_table c = cycle.get_or_create(get_self(), cycle_table());
    c.propcycle += 1;
    c.t_onperiod = current_time_point().sec_since_epoch();
    cycle.set(c, get_self());
}

void proposals::create(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund) {
  
  require_auth(creator);

  check_user(creator);

  check(fund == bankaccts::milestone || fund == bankaccts::alliances || fund == bankaccts::campaigns, 
  "Invalid fund - fund must be one of "+bankaccts::milestone.to_string() + ", "+ bankaccts::alliances.to_string() + ", " + bankaccts::campaigns.to_string() );

  if (fund == bankaccts::milestone) { // Milestone Seeds
    check(recipient == bankaccts::hyphabank, "Hypha proposals must go to " + bankaccts::hyphabank.to_string() + " - wrong recepient: " + recipient.to_string());
  } else {
    check(is_account(recipient), "recipient is not a valid account: " + recipient.to_string());
    check(is_account(fund), "fund is not a valid account: " + fund.to_string());
  }
  utils::check_asset(quantity);

  uint64_t lastId = 0;
  if (props.begin() != props.end()) {
    auto pitr = props.end();
    pitr--;
    lastId = pitr->id;
  }

  props.emplace(_self, [&](auto& proposal) {
      proposal.id = lastId + 1;
      proposal.creator = creator;
      proposal.recipient = recipient;
      proposal.quantity = quantity;
      proposal.staked = asset(0, seeds_symbol);
      proposal.executed = false;
      proposal.total = 0;
      proposal.favour = 0;
      proposal.against = 0;
      proposal.title = title;
      proposal.summary = summary;
      proposal.description = description;
      proposal.image = image;
      proposal.url = url;
      proposal.creation_date = eosio::current_time_point().sec_since_epoch();
      proposal.status = name("open");
      proposal.stage = name("staged");
      proposal.fund = fund;
  });

  auto litr = lastprops.find(creator.value);
  if (litr == lastprops.end()) {
    lastprops.emplace(_self, [&](auto& proposal) {
      proposal.account = creator;
      proposal.proposal_id = lastId+1;
    });
  } else {
    lastprops.modify(litr, _self, [&](auto& proposal) {
      proposal.account = creator;
      proposal.proposal_id = lastId+1;
    });
  }
}

void proposals::update(uint64_t id, string title, string summary, string description, string image, string url) {
  auto pitr = props.find(id);

  check(pitr != props.end(), "Proposal not found");
  require_auth(pitr->creator);
  check(pitr->favour == 0, "Prop has favor votes - cannot alter proposal once voting has started");
  check(pitr->against == 0, "Prop has against votes - cannot alter proposal once voting has started");

  props.modify(pitr, _self, [&](auto& proposal) {
    proposal.title = title;
    proposal.summary = summary;
    proposal.description = description;
    proposal.image = image;
    proposal.url = url;
  });
}

void proposals::cancel(uint64_t id) {
  auto pitr = props.find(id);
  check(pitr != props.end(), "Proposal not found");

  require_auth(pitr->creator);
  check(pitr->status == name("open"), "Proposal state is not open, it can no longer be cancelled");
  
  refund_staked(pitr->creator, pitr->staked);

  props.erase(pitr);

}

void proposals::stake(name from, name to, asset quantity, string memo) {
  if (to == _self) {
      utils::check_asset(quantity);
      //check_user(from);

      uint64_t id = 0;

      if (memo.empty()) {
        auto litr = lastprops.find(from.value);
        check(litr != lastprops.end(), "no proposals");

        id = litr->proposal_id;
      } else {
        id = std::stoi(memo);
      }

      auto pitr = props.find(id);
      check(pitr != props.end(), "no proposal");
      check(from == pitr->creator, "only the creator can stake into a proposal");

      props.modify(pitr, _self, [&](auto& proposal) {
          proposal.staked += quantity;
      });

      deposit(quantity);
  }
}

void proposals::erasepartpts(uint64_t active_proposals) {
  auto batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet.");
  auto reward_points = config.get(name("voterep1.ind").value, "The voterep1.ind parameter has not been initialized yet.");

  uint64_t counter = 0;
  auto pitr = participants.begin();
  while (pitr != participants.end() && counter < batch_size.value) {
    if (pitr -> count == active_proposals && pitr -> nonneutral) {
      action(
        permission_level{contracts::accounts, "active"_n},
        contracts::accounts, "addrep"_n,
        std::make_tuple(pitr -> account, reward_points.value)
      ).send();
    }
    counter += 1;
    pitr = participants.erase(pitr);
  }

  if (counter == batch_size.value) {
    transaction trx_erase_participants{};
    trx_erase_participants.actions.emplace_back(
      permission_level(_self, "active"_n),
      _self,
      "erasepartpts"_n,
      std::make_tuple(active_proposals)
    );
    // I don't know how long delay I should use
    trx_erase_participants.delay_sec = 5;
    trx_erase_participants.send(eosio::current_time_point().sec_since_epoch(), _self);
  }
}

void proposals::vote_aux (name voter, uint64_t id, uint64_t amount, name option) {
  require_auth(voter);
  check_citizen(voter);

  auto pitr = props.find(id);
  check(pitr != props.end(), "Proposal not found");
  check(pitr->executed == false, "Proposal was already executed");

  uint64_t min_stake = config.find(name("propminstake").value)->value;
  check(pitr->staked >= asset(min_stake, seeds_symbol), "not enough stake");
  check(pitr->stage == name("active"), "not active stage");

  auto vitr = voice.find(voter.value);
  check(vitr != voice.end(), "User does not have voice");
  check(vitr->balance >= amount, "voice balance exceeded");
  
  votes_tables votes(get_self(), id);
  auto voteitr = votes.find(voter.value);
  check(voteitr == votes.end(), "only one vote");

  check(option == trust || option == distrust || option == abstain, "Invalid option");

  if (option == trust) {
    props.modify(pitr, _self, [&](auto& proposal) {
      proposal.total += amount;
      proposal.favour += amount;
    });
    voice.modify(vitr, _self, [&](auto& voice) {
      voice.balance -= amount;
    });
  } else if (option == distrust) {
    props.modify(pitr, _self, [&](auto& proposal) {
      proposal.total += amount;
      proposal.against += amount;
    });
    voice.modify(vitr, _self, [&](auto& voice) {
      voice.balance -= amount;
    });
  }
  
  votes.emplace(_self, [&](auto& vote) {
    vote.account = voter;
    vote.amount = amount;
    if (option == trust) {
      vote.favour = true;
    } else if (option == distrust) {
      vote.favour = false;
    }
    vote.proposal_id = id;
  });

  auto rep = config.get(name("voterep2.ind").value, "The voterep2.ind parameter has not been initialized yet.");
  auto paitr = participants.find(voter.value);
  if (paitr == participants.end()) {
    // add reputation for entering in the table
    action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(voter, rep.value)
    ).send();
    // add the voter to the table
    participants.emplace(_self, [&](auto & participant){
      participant.account = voter;
      if (option == abstain) {
        participant.nonneutral = false;
      } else {
        participant.nonneutral = true;
      }
      participant.count = 1;
    });
  } else {
    participants.modify(paitr, _self, [&](auto & participant){
      participant.count += 1;
      if (option != abstain) {
        participant.nonneutral = true;
      }
    });
  }
}

void proposals::favour(name voter, uint64_t id, uint64_t amount) {
  vote_aux(voter, id, amount, trust);
}

void proposals::against(name voter, uint64_t id, uint64_t amount) {
  vote_aux(voter, id, amount, distrust);
}

void proposals::neutral(name voter, uint64_t id) {
  vote_aux(voter, id, (uint64_t)0, abstain);
}

void proposals::addvoice(name user, uint64_t amount)
{
    require_auth(_self);

    auto vitr = voice.find(user.value);

    if (vitr == voice.end()) {
        voice.emplace(_self, [&](auto& voice) {
            voice.account = user;
            voice.balance = amount;
        });
    } else {
        voice.modify(vitr, _self, [&](auto& voice) {
            voice.balance += amount;
        });
    }
}

void proposals::changetrust(name user, bool trust)
{
    require_auth(get_self());

    auto vitr = voice.find(user.value);

    if (vitr == voice.end() && trust) {
        voice.emplace(_self, [&](auto& voice) {
            voice.account = user;
            voice.balance = 0;
        });
    } else if (vitr != voice.end() && !trust) {
        voice.erase(vitr);
    }
}

void proposals::deposit(asset quantity)
{
  utils::check_asset(quantity);

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::transfer_action action{name(token_account), {_self, "active"_n}};
  action.send(_self, name(bank_account), quantity, "");
}

void proposals::refund_staked(name beneficiary, asset quantity) {
  withdraw(beneficiary, quantity, contracts::bank, "");
}
void proposals::change_rep(name beneficiary, bool passed) {
  if (passed) {
    auto reward_points = config.get(name("proppass.rep").value, "The proppass.rep parameter has not been initialized yet.");
    action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(beneficiary, reward_points.value)
    ).send();

  }

}

void proposals::send_to_escrow(name fromfund, name recipient, asset quantity, string memo) {

    action(
      permission_level{fromfund, "active"_n},
      contracts::token, "transfer"_n,
      std::make_tuple(fromfund, contracts::escrow, quantity, memo))
  .send();

  action(
      permission_level{fromfund, "active"_n},
      contracts::escrow, "lock"_n,
      std::make_tuple("event"_n, 
                      fromfund,
                      recipient,
                      quantity,
                      "golive"_n,
                      "dao.hypha"_n,
                      time_point(current_time_point().time_since_epoch() + 
                                  current_time_point().time_since_epoch()),  // long time from now
                      memo))
  .send();

}

void proposals::withdraw(name beneficiary, asset quantity, name sender, string memo)
{
  if (quantity.amount == 0) return;

  utils::check_asset(quantity);

  auto token_account = contracts::token;

  token::transfer_action action{name(token_account), {sender, "active"_n}};
  action.send(sender, beneficiary, quantity, memo);
}

void proposals::burn(asset quantity)
{
  utils::check_asset(quantity);

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::burn_action action{name(token_account), {name(bank_account), "active"_n}};
  action.send(name(bank_account), quantity);
}

void proposals::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}

void proposals::check_citizen(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
  check(uitr->status == name("citizen"), "user is not a citizen");
}
