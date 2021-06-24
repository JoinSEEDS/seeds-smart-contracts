#include <seeds.referendums.hpp>
#include <proposals/referendum_factory.hpp>

#include "referendums_settings.cpp"


ACTION referendums::reset () {
  require_auth(get_self());

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.begin();
  while (ritr != referendums_t.end()) {

    votes_tables votes_t(get_self(), ritr->referendum_id);
    auto vitr = votes_t.begin();
    while (vitr != votes_t.end()) {
      vitr = votes_t.erase(vitr);
    }

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
  check_attributes(args);

  std::unique_ptr<Referendum> ref = std::unique_ptr<Referendum>(ReferendumFactory::Factory(*this, type));

  ref->create(args);

}

ACTION referendums::update (std::map<std::string, VariantValue> & args) {

  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  require_auth(ritr->creator);
  check_attributes(args);

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

  auto staged_itr = referendums_by_stage_id.lower_bound(uint128_t(ReferendumsCommon::stage_staged.value) << 64);
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

  auto active_itr = referendums_by_stage_id.lower_bound(uint128_t(ReferendumsCommon::stage_active.value) << 64);
  while (active_itr != referendums_by_stage_id.end() && active_itr->stage == ReferendumsCommon::stage_active) {
    print("active referendum:", active_itr->referendum_id, "\n");
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
  bool vote_reverted = revert_vote(voter, referendum_id);
  vote_aux(voter, referendum_id, amount, ReferendumsCommon::distrust, !vote_reverted);
}

ACTION referendums::neutral (const name & voter, const uint64_t & referendum_id) {
  require_auth(voter);
  vote_aux(voter, referendum_id, uint64_t(0), ReferendumsCommon::neutral, false);
}


void referendums::vote_aux (const name & voter, const uint64_t & referendum_id, const uint64_t & amount, const name & option, const bool & is_new) {

  check_citizen(voter);

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  votes_tables votes_t(get_self(), referendum_id);
  auto vitr = votes_t.find(voter.value);
  check(vitr == votes_t.end(), "only one vote");

  if (is_new) {
    check(ritr->status == ReferendumsCommon::status_voting, "can not vote, it is not voting period");
  } else {
    check(ritr->stage == ReferendumsCommon::stage_active, "can not vote, referendum is not active");
  }

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
  // name scope = get_self();
  // send_inline_action(
  //   permission_level(contracts::voice, "active"_n),
  //   contracts::voice,
  //   "vote"_n,
  //   std::make_tuple(voter, amount, scope)
  // );

}

bool referendums::revert_vote (const name & voter, const uint64_t & referendum_id) {

  referendum_tables referendums_t(get_self(), get_self().value);
  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");

  votes_tables votes_t(get_self(), referendum_id);
  auto vitr = votes_t.find(voter.value);

  if (vitr != votes_t.end()) {
    check(ritr->stage == ReferendumsCommon::stage_active, "referendum is not active");
    check(vitr->favour == true && vitr->amount > 0, "only trust votes can be changed");

    referendums_t.modify(ritr, _self, [&](auto & item){
      item.favour -= vitr->amount;
    });

    votes_t.erase(vitr);

    return true;
  }

  return false;

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

