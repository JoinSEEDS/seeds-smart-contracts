#include <seeds.referendums.hpp>
#include <proposals/referendum_factory.hpp>

#include "referendums_settings.cpp"


ACTION referendums::reset () {
  require_auth(get_self());

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.begin();
  while (ritr != referendums_t.end()) {
    ritr = referendums_t.erase(ritr);
  }

  referendum_auxiliary_tables refaux_t(get_self(), get_self().value);
  auto raitr = refaux_t.begin();
  while (raitr != refaux_t.end()) {
    raitr = refaux_t.erase(raitr);
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

ACTION referendums::create (std::map<std::string, VariantValue> & args) {

  name creator = std::get<name>(args["creator"]);
  name type = std::get<name>(args["type"]);

  require_auth(creator);
  check_citizen(creator);

  std::unique_ptr<Referendum> ref = std::unique_ptr<Referendum>(ReferendumFactory::Factory(*this, type));

  ref->create(args);

}

ACTION referendums::update (std::map<std::string, VariantValue> & args) {

  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  require_auth(ritr->creator);

  std::unique_ptr<Referendum> ref = std::unique_ptr<Referendum>(ReferendumFactory::Factory(*this, ritr->type));

  ref->update(args);

}

ACTION referendums::cancel (std::map<std::string, VariantValue> & args) {

  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  require_auth(ritr->creator);

  std::unique_ptr<Referendum> ref = std::unique_ptr<Referendum>(ReferendumFactory::Factory(*this, ritr->type));

  ref->cancel(args);

}

ACTION referendums::stake (const name & from, const name & to, const asset & quantity, const string & memo) {

  if ( get_first_receiver() == contracts::token && 
       to == get_self() && 
       quantity.symbol == utils::seeds_symbol ) {
      
    utils::check_asset(quantity);

    uint64_t referendum_id = std::stoi(memo);

    referendum_tables referendums_t(get_self(), get_self().value);
    auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

    referendums_t.modify(ritr, _self, [&](auto & item){
      item.staked += quantity;
    });

  }

}

ACTION referendums::evaluate (const uint64_t & referendum_id) {

  require_auth(get_self());

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  std::unique_ptr<Referendum> ref = std::unique_ptr<Referendum>(ReferendumFactory::Factory(*this, ritr->type));

  std::map<string, VariantValue> args = {
    { string("referendum_id"), referendum_id }
  };

  ref->evaluate(args);

}

ACTION referendums::onperiod () {

  require_auth(get_self());

  referendum_tables referendums_t(get_self(), get_self().value);
  auto referendums_by_stage_id = referendums_t.get_index<"bystageid"_n>();

  print("running onperiod\n");

  auto staged_itr = referendums_by_stage_id.find(uint128_t(ReferendumsCommon::stage_staged.value) << 64);
  while (staged_itr != referendums_by_stage_id.end() && staged_itr->stage == ReferendumsCommon::stage_staged) {
    print("referendum:", staged_itr->referendum_id, "\n");
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(staged_itr->referendum_id)
    );
    staged_itr++;
  }

  auto active_itr = referendums_by_stage_id.find(uint128_t(ReferendumsCommon::stage_active.value) << 64);
  while (active_itr != referendums_by_stage_id.end() && active_itr->stage == ReferendumsCommon::stage_active) {
    send_deferred_transaction(
      permission_level(get_self(), "active"_n),
      get_self(),
      "evaluate"_n,
      std::make_tuple(active_itr->referendum_id)
    );
    active_itr++;
  }

}


ACTION referendums::favour (const name & voter, const uint64_t & referendum_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, referendum_id, amount, ReferendumsCommon::trust, false);
}

ACTION referendums::against (const name & voter, const uint64_t & referendum_id, const uint64_t & amount) {
  require_auth(voter);
  vote_aux(voter, referendum_id, amount, ReferendumsCommon::distrust, false); // this will change when implementing the change vote functionality
}

ACTION referendums::neutral (const name & voter, const uint64_t & referendum_id) {
  require_auth(voter);
  vote_aux(voter, referendum_id, uint64_t(0), ReferendumsCommon::neutral, false);
}


void referendums::vote_aux (const name & voter, const uint64_t & referendum_id, const uint64_t & amount, const name & option, const bool & is_new) {

  // do some logic related to the referendums
  check_citizen(voter);

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  if (is_new) {
    check(ritr->status == ReferendumsCommon::status_voting, "can not vote, it is not voting period");
  } else {
    check(ritr->stage == ReferendumsCommon::stage_active, "can not vote, referendum is not active");
    // this case happens when a user reverts its vote
    // some logic here
  }

  votes_tables votes_t(get_self(), referendum_id);
  auto vitr = votes_t.find(voter.value);
  check(vitr == votes_t.end(), "only one vote");

  if (option == ReferendumsCommon::trust) {
    referendums_t.modify(ritr, _self, [&](auto & item){
      item.favour += amount;
    });
  } else if (option == ReferendumsCommon::distrust) {
    referendums_t.modify(ritr, _self, [&](auto & item){
      item.against += amount;
    });
  }

  votes_t.emplace(_self, [&](auto & item){
    item.proposal_id = referendum_id;
    item.account = voter;
    item.amount = amount;
    item.favour = option == ReferendumsCommon::trust;
  });

  // storing the number of voters per referendum in a separate scope
  size_tables sizes_t(get_self(), ReferendumsCommon::vote_scope.value);
  auto sitr = sizes_t.find(referendum_id);

  if (sitr == sizes_t.end()) {
    sizes_t.emplace(_self, [&](auto & item){
      item.id = name(referendum_id);
      item.size = 1;
    });
  } else {
    sizes_t.modify(sitr, _self, [&](auto & item){
      item.size += 1;
    });
  }

  // send reduce voice
  name scope = get_self();
  send_inline_action(
    permission_level(contracts::voice, "active"_n),
    contracts::voice,
    "vote"_n,
    std::make_tuple(voter, amount, scope)
  );

}


// void referendums::give_voice() {
//   auto bitr = balances.begin();

//   while (bitr != balances.end()) {
//     balances.modify(bitr, get_self(), [&](auto& item) {
//       item.voice = 10;
//     });

//     bitr++;
//   }
// }


// void referendums::addvoice(name account, uint64_t amount) {
//   auto bitr = balances.find(account.value);

//   if (bitr == balances.end()) {
//     balances.emplace(get_self(), [&](auto& balance) {
//       balance.account = account;
//       balance.voice = amount;
//       balance.stake = asset(0, seeds_symbol);
//     });
//   } else {
//     balances.modify(bitr, get_self(), [&](auto& balance) {
//       balance.voice += amount;
//     });
//   }
// }

// void referendums::favour(name voter, uint64_t referendum_id, uint64_t amount) {
//   auto bitr = balances.find(voter.value);
//   check(bitr != balances.end(), "user has no voice");
//   check(bitr->voice >= amount, "not enough voice");

//   voter_tables voters(get_self(), referendum_id);
//   auto vitr = voters.find(voter.value);
//   check(vitr == voters.end(), "user can only vote one time");

//   referendum_tables active(get_self(), name("active").value);
//   auto aitr = active.find(referendum_id);
//   check (aitr != active.end(), "referendum does not exist");

//   balances.modify(bitr, get_self(), [&](auto& balance) {
//     balance.voice -= amount;
//   });

//   active.modify(aitr, get_self(), [&](auto& referendum) {
//     referendum.favour += amount;
//   });

//   voters.emplace(get_self(), [&](auto& item) {
//     item.account = voter;
//     item.referendum_id = referendum_id;
//     item.amount = amount;
//     item.favoured = true;
//     item.canceled = false;
//   });
// }

// void referendums::against(name voter, uint64_t referendum_id, uint64_t amount) {
//   require_auth(voter);

//   auto bitr = balances.find(voter.value);
//   check(bitr != balances.end(), "user has no voice");
//   check(bitr->voice >= amount, "not enough voice");

//   referendum_tables active(get_self(), name("active").value);
//   auto aitr = active.find(referendum_id);
//   check(aitr != active.end(), "referendum not found");

//   voter_tables voters(get_self(), referendum_id);
//   auto vitr = voters.find(voter.value);
//   check(vitr == voters.end(), "user can only vote one time");

//   balances.modify(bitr, get_self(), [&](auto& balance) {
//     balance.voice -= amount;
//   });

//   active.modify(aitr, get_self(), [&](auto& item) {
//     item.against += amount;
//   });

//   voters.emplace(get_self(), [&](auto& item) {
//     item.account = voter;
//     item.referendum_id = referendum_id;
//     item.amount = amount;
//     item.favoured = false;
//     item.canceled = false;
//   });
// }

// void referendums::cancelvote(name voter, uint64_t referendum_id) {
//   require_auth(voter);

//   voter_tables voters(get_self(), referendum_id);
//   auto vitr = voters.find(voter.value);
//   check(vitr != voters.end(), "vote not found");
//   check(vitr->canceled == false, "vote already canceled");

//   referendum_tables testing(get_self(), name("testing").value);
//   auto titr = testing.find(referendum_id);
//   check(titr != testing.end(), "testing referendum not found");

//   voters.modify(vitr, get_self(), [&](auto& voter) {
//     voter.canceled = true;
//   });

//   if (vitr->favoured == true) {
//     testing.modify(titr, get_self(), [&](auto& item) {
//       item.favour -= vitr->amount;
//     });
//   } else {
//     testing.modify(titr, get_self(), [&](auto& item) {
//       item.against -= vitr->amount;
//     });
//   }
// }


void referendums::check_citizen(const name & account) {
  DEFINE_USER_TABLE;
  DEFINE_USER_TABLE_MULTI_INDEX;
  user_tables users(contracts::accounts, contracts::accounts.value);

  auto uitr = users.find(account.value);
  check(uitr != users.end(), "user not found");
  check(uitr->status == name("citizen"), "user is not a citizen");
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

  eosio::action(
    permission,
    contract,
    action,
    data
  ).send();

  // deferred_id_tables deferredids(get_self(), get_self().value);
  // deferred_id_table d_s = deferredids.get_or_create(get_self(), deferred_id_table());

  // d_s.id += 1;

  // deferredids.set(d_s, get_self());

  // transaction trx{};

  // trx.actions.emplace_back(
  //   permission,
  //   contract,
  //   action,
  //   data
  // );

  // trx.delay_sec = 1;
  // trx.send(d_s.id, _self);

}

