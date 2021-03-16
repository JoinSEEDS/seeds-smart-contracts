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

  voice_tables voice_alliance(get_self(), alliance_type.value);
  auto vaitr = voice_alliance.begin();
  while (vaitr != voice_alliance.end()) {
    vaitr = voice_alliance.erase(vaitr);
  }

  auto paitr = participants.begin();
  while (paitr != participants.end()) {
    paitr = participants.erase(paitr);
  }

  auto mitr = minstake.begin();
  while (mitr != minstake.end()) {
    mitr = minstake.erase(mitr);
  }

  auto aitr = actives.begin();
  while (aitr != actives.end()) {
    aitr = actives.erase(aitr);
  }

  size_tables sizes(get_self(), get_self().value);
  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }

  name scopes[] = { get_self(), alliance_type };
  for (int i = 0; i < 2; i++) {
    delegate_trust_tables deltrusts(get_self(), (scopes[i]).value);
    auto ditr = deltrusts.begin();
    while (ditr != deltrusts.end()) {
      ditr = deltrusts.erase(ditr);
    }
  }

  auto citr = cyclestats.begin();
  while (citr != cyclestats.end()) {
    citr = cyclestats.erase(citr);
  }

  cycle.remove();

}

bool proposals::is_enough_stake(asset staked, asset quantity, name fund) {
  uint64_t min = min_stake(quantity, fund);
  return staked.amount >= min;
}

uint64_t proposals::cap_stake(name fund) {
  uint64_t prop_max;
  if (fund == bankaccts::campaigns) {
    prop_max = config_get(name("prop.cmp.cap"));
  } else if (fund == bankaccts::alliances) {
    prop_max = config_get(name("prop.al.cap"));
  } else {
    prop_max = config_get(name("propmaxstake"));
  }
  return prop_max;
}

uint64_t proposals::min_stake(asset quantity, name fund) {

  double prop_percentage;
  uint64_t prop_min;
  uint64_t prop_max;

  
  if (fund == bankaccts::campaigns) {
    prop_percentage = (double)config_get(name("prop.cmp.pct")) / 10000.0;
    prop_max = config_get(name("prop.cmp.cap"));
    prop_min = config_get(name("prop.cmp.min"));
  } else if (fund == bankaccts::alliances) {
    prop_percentage = (double)config_get(name("prop.al.pct")) / 10000.0;
    prop_max = config_get(name("prop.al.cap"));
    prop_min = config_get(name("prop.al.min"));
  } else if (fund == bankaccts::milestone) {
    prop_percentage = (double)config_get(name("propstakeper"));
    prop_max = config_get(name("propminstake"));
    prop_min = config_get(name("propminstake"));
  } else {
    check(false, "unknown proposal type, invalid fund");
  }

  asset quantity_prop_percentage = asset(uint64_t(prop_percentage * quantity.amount / 100), seeds_symbol);

  uint64_t min_stake = std::max(uint64_t(prop_min), uint64_t(quantity_prop_percentage.amount));
  min_stake = std::min(prop_max, min_stake);
  return min_stake;
}

ACTION proposals::checkstake(uint64_t prop_id) {
  auto pitr = props.find(prop_id);
  check(pitr != props.end(), "proposal not found");
  check(is_enough_stake(pitr->staked, pitr->quantity, pitr->fund), "{ 'error':'not enough stake', 'has':" + std::to_string(pitr->staked.amount) + "', 'min_stake':'"+std::to_string(min_stake(pitr->quantity, pitr->fund)) + "' }");
}

void proposals::update_min_stake(uint64_t prop_id) {

  auto pitr = props.find(prop_id);
  check(pitr != props.end(), "proposal not found");

  uint64_t min = min_stake(pitr->quantity, pitr->fund);

  auto mitr = minstake.find(prop_id);
  if (mitr == minstake.end()) {
    minstake.emplace(_self, [&](auto& item) {
      item.prop_id = prop_id;
      item.min_stake = min;
    });
  } else {
    minstake.modify(mitr, _self, [&](auto& item) {
      item.min_stake = min;
    });
  }
}

// quorum as integer % value - e.g. 90 == 90%
uint64_t proposals::get_quorum(uint64_t total_proposals) {
  uint64_t base_quorum = config_get("quorum.base"_n);
  uint64_t quorum_min = config_get("quor.min.pct"_n);
  uint64_t quorum_max = config_get("quor.max.pct"_n);

  uint64_t quorum = total_proposals ? base_quorum / total_proposals : 0;
  quorum = std::max(quorum_min, quorum);
  return std::min(quorum_max, quorum);
}

void proposals::testquorum(uint64_t total_proposals) {
  require_auth(get_self());
  check(false, std::to_string(get_quorum(total_proposals)));
}

asset proposals::get_payout_amount(
  std::vector<uint64_t> pay_percentages, 
  uint64_t age, 
  asset total_amount, 
  asset current_payout
) {

  if (age >= pay_percentages.size()) { return asset(0, seeds_symbol); }

  if (age == pay_percentages.size() - 1) {
    return total_amount - current_payout;
  }

  double payout_percentage = pay_percentages[age] / 100.0;
  uint64_t payout_amount = payout_percentage * total_amount.amount;
  
  return asset(payout_amount, seeds_symbol);

}

uint64_t proposals::get_size(name id) {
  size_tables sizes(get_self(), get_self().value);

  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}

void proposals::initsz() {
  require_auth(_self);

  uint64_t cutoff_date = active_cutoff_date();

  int64_t count = 0; 
  auto aitr = actives.begin();
  while(aitr != actives.end()) {
    if (aitr -> timestamp >= cutoff_date) {
      count++;
    }
    aitr++;
  }
  print("size change "+std::to_string(count));
  size_set(user_active_size, count);
}

void proposals::calcvotepow() {
  require_auth(_self);
  
  // remove unused size
  size_tables sizes(get_self(), get_self().value);
  auto sitr = sizes.find("active.sz"_n.value);
  if (sitr != sizes.end()) {
    sizes.erase(sitr);
  }
  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX

  cs_points_tables cspoints(contracts::harvest, contracts::harvest.value);
  uint64_t cutoff_date = active_cutoff_date();
  uint64_t vote_power = 0;
  uint64_t voice_size = 0;

  auto vitr = voice.begin();
  while(vitr != voice.end()) {
    if (is_active(vitr->account, cutoff_date)) {
      auto csitr = cspoints.find(vitr->account.value);
      uint64_t points = 0;
      if (csitr != cspoints.end()) {
        points = csitr -> rank;
      }

      vote_power += points;
      
      print("| active: " + 
        vitr->account.to_string() +
        " pt: " + std::to_string(points) + " " 
        " total: " + std::to_string(vote_power) + " " 
      );
    } else {
      print(" inactive: "+vitr->account.to_string() + " ");
    }
    voice_size++;
    vitr++;
  }

  //size_set(cycle_vote_power_size, vote_power);
  //size_set("voice.sz"_n, voice_size);

}

uint64_t proposals::active_cutoff_date() {
  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t prop_cycle_sec = config_get(name("propcyclesec"));
  uint64_t inact_cycles = config_get(name("inact.cyc"));
  return now - (inact_cycles * prop_cycle_sec);
}

bool proposals::is_active(name account, uint64_t cutoff_date) {
  auto aitr = actives.find(account.value);
  return aitr != actives.end() && aitr->timestamp > cutoff_date;
}

void proposals::onperiod() {
    require_auth(_self);

    auto props_by_status = props.get_index<"bystatus"_n>();

    uint64_t prop_majority = config_get(name("propmajority"));

    uint64_t number_active_proposals = get_size(prop_active_size);
    uint64_t total_eligible_voters = get_size(user_active_size);
    check(total_eligible_voters > 0, "no eligible voters - likely an error; can't run proposals.");
    
    uint64_t quorum =  get_quorum(number_active_proposals);

    cycle_table c = cycle.get_or_create(get_self(), cycle_table());
    uint64_t current_cycle = c.propcycle;

    std::vector<uint64_t> active_props;
    std::vector<uint64_t> eval_props;

    // TODO this is not working at the moment, use old way... FIX after this cycle.

    // find smallesd prop id that's in open or eval stage
    // this way we skip all proposals that are definitely already passed or rejected
    // auto pps_itr = props_by_status.begin();
    // uint64_t smallest_prop_id = 0;
    // while (pps_itr != props_by_status.end() && 
    //   (pps_itr -> status == status_open || pps_itr -> status == status_evaluate) ) {
    //   smallest_prop_id  = std::min(smallest_prop_id, pps_itr -> id);
    //   pps_itr++;
    // }

    // print("smallest id: "+std::to_string(smallest_prop_id));
      
    // auto pitr = props.find(smallest_prop_id);

    auto pitr = props.begin();
    
    while (pitr != props.end()) {
      uint64_t prop_id = pitr -> id;

      // active proposals are evaluated
      if (pitr->stage == stage_active) {

        votes_tables votes(get_self(), pitr->id);

        double majority = double(prop_majority) / 100.0;
        double fav = double(pitr->favour);
        bool passed = pitr->favour > 0 && fav >= double(pitr->favour + pitr->against) * majority;
        name prop_type = get_type(pitr->fund);
        bool is_alliance_type = prop_type == alliance_type;
        bool is_campaign_type = prop_type == campaign_type;

        bool valid_quorum = false;

        if (pitr->status == status_evaluate) { // in evaluate status, we only check unity. 
          valid_quorum = true;
        } else { // in open status, quorum is calculated
          uint64_t votes_in_favor = pitr->favour; // only votes in favor are counted
          auto citr = cyclestats.find(current_cycle);
          uint64_t quorum_votes_needed = citr != cyclestats.end() ? citr->quorum_votes_needed : 0;
          valid_quorum = votes_in_favor >= quorum_votes_needed;
        }

        if (passed && valid_quorum) {

          if (pitr -> status == status_open) {

            refund_staked(pitr->creator, pitr->staked);
            change_rep(pitr->creator, true);

            asset payout_amount = get_payout_amount(pitr->pay_percentages, 0, pitr->quantity, pitr->current_payout);
            
            if (is_alliance_type) {
              send_to_escrow(pitr->fund, pitr->recipient, payout_amount, "proposal id: "+std::to_string(pitr->id));
            } else {
              withdraw(pitr->recipient, payout_amount, pitr->fund, "");// TODO limit by amount available
            }

            // TODO: if we allow num_cycles == 1, this needs to go into passed instead of evaluate.
            // uint64_t num_cycles = pitr -> pay_percentages.size() - 1;

            props.modify(pitr, _self, [&](auto & proposal){
              proposal.passed_cycle = current_cycle;
              proposal.age = 0;
              proposal.staked = asset(0, seeds_symbol);
              proposal.status = status_evaluate;
              proposal.current_payout += payout_amount;
            });

            eval_props.push_back(prop_id);

          } else {
            
            uint64_t age = pitr -> age + 1;

            asset payout_amount = get_payout_amount(pitr->pay_percentages, age, pitr->quantity, pitr->current_payout);

            if (is_alliance_type) {
              send_to_escrow(pitr->fund, pitr->recipient, payout_amount, "proposal id: "+std::to_string(pitr->id));
            } else {
              withdraw(pitr->recipient, payout_amount, pitr->fund, "");// TODO limit by amount available
            }

            uint64_t num_cycles = pitr -> pay_percentages.size() - 1;

            props.modify(pitr, _self, [&](auto & proposal){
              proposal.age = age;
              if (age == num_cycles) {
                proposal.executed = true;
                proposal.status = status_passed;
                proposal.stage = stage_done;
              } else {
                eval_props.push_back(prop_id);
              }
              proposal.current_payout += payout_amount;
            });
          }

        } else {
          if (pitr->status != status_evaluate) {
            burn(pitr->staked);
          }

          props.modify(pitr, _self, [&](auto& proposal) {
              if (pitr->status != status_evaluate) {
                proposal.passed_cycle = current_cycle;
              }
              proposal.executed = false;
              proposal.staked = asset(0, seeds_symbol);
              proposal.status = status_rejected;
              proposal.stage = stage_done;
          });
        }

        size_change(prop_active_size, -1);
      
      }
      
      // staged proposals become active if there's enough stake
      if (pitr->stage == stage_staged && is_enough_stake(pitr->staked, pitr->quantity, pitr->fund) ) {
        props.modify(pitr, _self, [&](auto& proposal) {
          proposal.stage = stage_active;
        });
        size_change(prop_active_size, 1);
        active_props.push_back(prop_id);
      }

      pitr++;
    } 

    update_cycle();
    update_cycle_stats(active_props, eval_props);
    updatevoices();
    
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
}

void proposals::testperiod() {
    require_auth(_self);

    auto props_by_status = props.get_index<"bystatus"_n>();
    uint64_t prop_majority = config_get(name("propmajority"));
    uint64_t number_active_proposals = get_size(prop_active_size);
    uint64_t total_eligible_voters = get_size(user_active_size);
    check(total_eligible_voters > 0, "no eligible voters - likely an error; can't run proposals.");
    
    uint64_t quorum =  get_quorum(number_active_proposals);

    cycle_table c = cycle.get_or_create(get_self(), cycle_table());
    uint64_t current_cycle = c.propcycle;

    std::vector<uint64_t> active_props;
    std::vector<uint64_t> eval_props;

    // TODO this is not working at the moment, use old way... FIX after this cycle.

    // find smallesd prop id that's in open or eval stage
    // this way we skip all proposals that are definitely already passed or rejected
    // auto pps_itr = props_by_status.begin();
    // uint64_t smallest_prop_id = 0;
    // while (pps_itr != props_by_status.end() && 
    //   (pps_itr -> status == status_open || pps_itr -> status == status_evaluate) ) {
    //   smallest_prop_id  = std::min(smallest_prop_id, pps_itr -> id);
    //   pps_itr++;
    // }

    // print("smallest id: "+std::to_string(smallest_prop_id) );
      
    // auto pitr = props.find(smallest_prop_id);
    auto pitr = props.find(70);

    while (pitr != props.end()) {
      uint64_t prop_id = pitr -> id;

      print(" checking id: "+std::to_string(prop_id));

      // active proposals are evaluated
      if (pitr->stage == stage_active) {

        votes_tables votes(get_self(), pitr->id);
        
        double majority = double(prop_majority) / 100.0;
        double fav = double(pitr->favour);
        bool passed = pitr->favour > 0 && fav >= double(pitr->favour + pitr->against) * majority;
        bool valid_quorum = false;

        if (pitr->status == status_evaluate) { // in evaluate status, we only check unity. 
          valid_quorum = true;
        } else { // in open status, quorum is calculated
          uint64_t votes_in_favor = pitr->favour; // only votes in favor are counted
          auto citr = cyclestats.find(current_cycle);
          uint64_t quorum_votes_needed = citr != cyclestats.end() ? citr->quorum_votes_needed : 0;
          valid_quorum = votes_in_favor >= quorum_votes_needed;

          print(
            " prop ID " + std::to_string(pitr->id) +
            " vp favor " + std::to_string(votes_in_favor) +
            " needed: " + std::to_string(quorum_votes_needed) +
            " valid: " + ( valid_quorum ? "YES " : "NO ") 
          );

        }

        if (passed && valid_quorum) {
          
          print(" passed ");

        } else {
            print(" failed ");
        }
      
      }
      pitr++;
    } 
}
void proposals::updatevoices() {
  require_auth(get_self());
  updatevoice((uint64_t)0);
}

void proposals::updatevoice(uint64_t start) {
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

      set_voice(vitr -> account, points, ""_n);

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

uint64_t proposals::get_cycle_period_sec() {
  auto moon_cycle = config_get(name("mooncyclesec"));
  return moon_cycle / 2; // Using half moon cycles for now
}

uint64_t proposals::get_voice_decay_period_sec() {
  auto voice_decay_period = config_get(name("propdecaysec"));
  return voice_decay_period;
}

void proposals::decayvoices() {
  require_auth(get_self());

  cycle_table c = cycle.get_or_create(get_self(), cycle_table());

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

void proposals::decayvoice(uint64_t start, uint64_t chunksize) {
  require_auth(get_self());

  voice_tables voice_alliance(get_self(), alliance_type.value);

  uint64_t percentage_decay = config_get(name("vdecayprntge"));
  check(percentage_decay <= 100, "Voice decay parameter can not be more than 100%.");
  auto vitr = start == 0 ? voice.begin() : voice.find(start);
  uint64_t count = 0;

  double multiplier = (100.0 - (double)percentage_decay) / 100.0;

  while (vitr != voice.end() && count < chunksize) {
    auto vaitr = voice_alliance.find(vitr -> account.value);

    voice.modify(vitr, _self, [&](auto & v){
      v.balance *= multiplier;
    });

    if (vaitr != voice_alliance.end()) {
      voice_alliance.modify(vaitr, _self, [&](auto & va){
        va.balance *= multiplier;
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

void proposals::migratevoice(uint64_t start) {
  require_auth(get_self());

  auto vitr = start == 0 ? voice.begin() : voice.find(start);
  uint64_t chunksize = 200;
  uint64_t count = 0;

  voice_tables voice_alliance(get_self(), alliance_type.value);

  while (vitr != voice.end() && count < chunksize) {
    auto vaitr = voice_alliance.find(vitr -> account.value);
    if (vaitr == voice_alliance.end()) {
      voice_alliance.emplace(_self, [&](auto & voice){
        voice.account = vitr -> account;
        voice.balance = vitr -> balance;
      });
    }
    vitr++;
    count++;
  }

  if (vitr != voice.end()) {
    uint64_t next_value = vitr -> account.value;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "migratevoice"_n,
        std::make_tuple(next_value)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void proposals::update_cycle() {
    cycle_table c = cycle.get_or_create(get_self(), cycle_table());
    c.propcycle += 1;
    c.t_onperiod = current_time_point().sec_since_epoch();
    cycle.set(c, get_self());
}

void proposals::update_cycle_stats (std::vector<uint64_t>active_props, std::vector<uint64_t> eval_props) {
  cycle_table c = cycle.get();

  uint64_t quorum_vote_base = calc_quorum_base(c.propcycle - 1);
  uint64_t num_proposals = active_props.size();

  cyclestats.emplace(_self, [&](auto & item){
    item.propcycle = c.propcycle;
    item.start_time = c.t_onperiod;
    item.end_time = c.t_onperiod + config_get("propcyclesec"_n);
    item.num_proposals = num_proposals;
    item.num_votes = 0;
    item.total_voice_cast = 0;
    item.total_favour = 0;
    item.total_against = 0;
    item.total_citizens = get_size("voice.sz"_n);
    item.quorum_vote_base = quorum_vote_base;
    item.quorum_votes_needed = quorum_vote_base * (get_quorum(num_proposals) / 100.0);
    item.unity_needed = double(config_get("propmajority"_n)) / 100.0;
    item.active_props.assign(active_props.begin(), active_props.end());
    item.eval_props.assign(eval_props.begin(), eval_props.end());
  });

}

void proposals::create(
  name creator, 
  name recipient, 
  asset quantity, 
  string title, 
  string summary, 
  string description, 
  string image, 
  string url, 
  name fund
) {
  require_auth(creator);
  std::vector<uint64_t> perc = { 25, 25, 25, 25 };

  createx(creator, recipient, quantity, title, summary, description, image, url, fund, perc );
}

void proposals::createx(
  name creator, 
  name recipient, 
  asset quantity, 
  string title, 
  string summary, 
  string description, 
  string image, 
  string url, 
  name fund,
  std::vector<uint64_t> pay_percentages
) {
  
  require_auth(creator);

  check_user(creator);
  
  check_percentages(pay_percentages);

  check(get_type(fund) != "none"_n, 
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
  
  uint64_t propKey = lastId + 1;

  props.emplace(_self, [&](auto& proposal) {
      proposal.id = propKey;
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
      proposal.status = status_open;
      proposal.stage = stage_staged;
      proposal.fund = fund;
      proposal.pay_percentages = pay_percentages;
      proposal.passed_cycle = 0;
      proposal.age = 0;
      proposal.current_payout = asset(0, seeds_symbol);
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
  update_min_stake(propKey);
}

void proposals::update(uint64_t id, string title, string summary, string description, string image, string url) {
  auto pitr = props.find(id);
  check(pitr != props.end(), "Proposal not found");

  updatex(id, title, summary, description, image, url, pitr->pay_percentages);
}

void proposals::check_percentages(std::vector<uint64_t> pay_percentages) {
  uint64_t num_cycles = pay_percentages.size() - 1;
  check(num_cycles >= 3, "the number of cycles is to small, it must be minumum 3, given:" + std::to_string(num_cycles));
  check(num_cycles <= 24, "the number of cycles is to big, it must be maximum 24, given:" + std::to_string(num_cycles));
  uint64_t sum = 0;
  for(std::size_t i = 0; i < pay_percentages.size(); ++i) {
    sum += pay_percentages[i];
  }
  check(sum == 100, "percentages must add up to 100");
  uint64_t initial_payout = pay_percentages[0];
  check(initial_payout <= 25, "the initial payout must be smaller than 25%, given:" + std::to_string(initial_payout));
  check(num_cycles <= 24, "the number of cycles is to big, it must be maximum 24, given:" + std::to_string(num_cycles));
}

void proposals::updatex(uint64_t id, string title, string summary, string description, string image, string url, std::vector<uint64_t> pay_percentages) {
  auto pitr = props.find(id);

  check(pitr != props.end(), "Proposal not found");
  require_auth(pitr->creator);
  check(pitr->favour == 0, "Prop has favor votes - cannot alter proposal once voting has started");
  check(pitr->against == 0, "Prop has against votes - cannot alter proposal once voting has started");

  check_percentages(pay_percentages);

  props.modify(pitr, _self, [&](auto& proposal) {
    proposal.title = title;
    proposal.summary = summary;
    proposal.description = description;
    proposal.image = image;
    proposal.url = url;
    proposal.pay_percentages = pay_percentages;
  });
}

void proposals::cancel(uint64_t id) {
  auto pitr = props.find(id);
  check(pitr != props.end(), "Proposal not found");

  require_auth(pitr->creator);
  check(pitr->status == status_open, "Proposal state is not open, it can no longer be cancelled");
  
  refund_staked(pitr->creator, pitr->staked);

  props.erase(pitr);

}

void proposals::stake(name from, name to, asset quantity, string memo) {
  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
        to  ==  get_self() &&                     // to here
        quantity.symbol == seeds_symbol) {        // SEEDS symbol

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
      // now, other people can stake for other people - code change 2020-11-13
      //check(from == pitr->creator, "only the creator can stake into a proposal");

      uint64_t prop_max = cap_stake(pitr->fund);
      check((pitr -> staked + quantity) <= asset(prop_max, seeds_symbol), 
        "The staked value can not be greater than " + std::to_string(prop_max / 10000) + " Seeds");

      props.modify(pitr, _self, [&](auto& proposal) {
          proposal.staked += quantity;
      });

      deposit(quantity);
  }
}

void proposals::erasepartpts(uint64_t active_proposals) {
  uint64_t batch_size = config_get(name("batchsize"));
  uint64_t reward_points = config_get(name("voterep1.ind"));

  uint64_t counter = 0;
  auto pitr = participants.begin();
  while (pitr != participants.end() && counter < batch_size) {
    if (pitr -> count == active_proposals && pitr -> nonneutral) {
      action(
        permission_level{contracts::accounts, "active"_n},
        contracts::accounts, "addrep"_n,
        std::make_tuple(pitr -> account, reward_points)
      ).send();
    }
    counter += 1;
    pitr = participants.erase(pitr);
  }

  if (counter == batch_size) {
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

bool proposals::revert_vote (name voter, uint64_t id) {
  auto pitr = props.find(id);
  
  check(pitr != props.end(), "Proposal not found");

  votes_tables votes(get_self(), id);
  auto voteitr = votes.find(voter.value);

  if (voteitr != votes.end()) {
    check(pitr->status == status_evaluate, "Proposal is not in evaluate state");
    check(voteitr->favour == true && voteitr->amount > 0, "Only trust votes can be changed");

    props.modify(pitr, _self, [&](auto& proposal) {
      proposal.total -= voteitr->amount;
      proposal.favour -= voteitr->amount;
    });

    votes.erase(voteitr);
    return true;
  }

  return false;
}

void proposals::vote_aux (name voter, uint64_t id, uint64_t amount, name option, bool is_new, bool is_delegated) {
  check_citizen(voter);

  auto pitr = props.find(id);
  check(pitr != props.end(), "Proposal not found");
  check(pitr->executed == false, "Proposal was already executed");

  check(pitr->stage == stage_active, "not active stage");

  if (is_new) {
    check(pitr->status == status_open, "the user " + voter.to_string() + " can not vote for this proposal, as the proposal is in evaluate state");
  }

  votes_tables votes(get_self(), id);
  auto voteitr = votes.find(voter.value);

  check(voteitr == votes.end(), "only one vote");
  
  check(option == trust || option == distrust || option == abstain, "Invalid option");

  if (option == trust) {
    props.modify(pitr, _self, [&](auto& proposal) {
      proposal.total += amount;
      proposal.favour += amount;
    });
  } else if (option == distrust) {
    props.modify(pitr, _self, [&](auto& proposal) {
      proposal.total += amount;
      proposal.against += amount;
    });
  }

  name scope;
  name fund_type = get_type(pitr -> fund);
  if (fund_type == alliance_type) {
    scope = alliance_type;
  } else {
    scope = get_self();
  }

  double percenetage_used = voice_change(voter, amount, true, scope);
  
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

  if (!is_delegated) {
    check(!is_trust_delegated(voter, scope), "voice is delegated, user can not vote by itself");
  }

  send_mimic_delegatee_vote(voter, scope, id, percenetage_used, option);

  if (is_new) {
    auto rep = config_get(name("voterep2.ind"));
    double rep_multiplier = is_delegated ? config_get(name("votedel.mul")) / 100.0 : 1.0;
    auto paitr = participants.find(voter.value);
    if (paitr == participants.end()) {
      // add reputation for entering in the table
      action(
        permission_level{contracts::accounts, "active"_n},
        contracts::accounts, "addrep"_n,
        std::make_tuple(voter, uint64_t(rep * rep_multiplier))
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

  auto aitr = actives.find(voter.value);
  if (aitr == actives.end()) {
    actives.emplace(_self, [&](auto& item) {
      item.account = voter;
      item.timestamp = current_time_point().sec_since_epoch();
    });
    size_change(user_active_size, 1);
  } else {
    actives.modify(aitr, _self, [&](auto & item){
      item.timestamp = current_time_point().sec_since_epoch();
    });
  }

  add_voted_proposal(pitr->id); // this should happen in onperiod, when status is set to open / active
  increase_voice_cast(voter, amount, option);

}

void proposals::favour(name voter, uint64_t id, uint64_t amount) {
  require_auth(voter);
  vote_aux(voter, id, amount, trust, true, false);
}

void proposals::against(name voter, uint64_t id, uint64_t amount) {
  require_auth(voter);
  bool vote_reverted = revert_vote(voter, id);
  vote_aux(voter, id, amount, distrust, !vote_reverted, false);
}

void proposals::neutral(name voter, uint64_t id) {
  require_auth(voter);
  vote_aux(voter, id, (uint64_t)0, abstain, true, false);
}

void proposals::voteonbehalf(name voter, uint64_t id, uint64_t amount, name option) {
  require_auth(get_self());
  bool is_new = true;
  if (option == distrust) {
    is_new = !(revert_vote(voter, id));
  }
  vote_aux(voter, id, amount, option, is_new, true);
}

void proposals::addvoice(name user, uint64_t amount) {
  require_auth(_self);
  voice_change(user, amount, false, ""_n);
}

double proposals::voice_change (name user, uint64_t amount, bool reduce, name scope) {
  double percentage_used = 0.0;

  if (scope == ""_n) {
    voice_tables voice_alliance(get_self(), alliance_type.value);

    auto vitr = voice.find(user.value);
    auto vaitr = voice_alliance.find(user.value);

    if (vitr == voice.end() && vaitr == voice_alliance.end()) {
      check(!reduce, "user can not have negative voice balance");
      voice.emplace(_self, [&](auto & voice) {
        voice.account = user;
        voice.balance = amount;
      });
      size_change("voice.sz"_n, 1);
      voice_alliance.emplace(_self, [&](auto & voice){
        voice.account = user;
        voice.balance = amount;
      });
    } else if (vitr != voice.end() && vaitr != voice_alliance.end()) {
      if (reduce) {
        check(amount <= vitr -> balance && amount <= vaitr -> balance, "voice balance exceeded");
        percentage_used = amount / double(vitr -> balance);
      }
      voice.modify(vitr, _self, [&](auto& voice) {
        if (reduce) {
          voice.balance -= amount;
        } else {
          voice.balance += amount;
        }
      });
      voice_alliance.modify(vaitr, _self, [&](auto & voice){
        if (reduce) {
          voice.balance -= amount;
        } else {
          voice.balance += amount;
        }
      });
    }
  } else {
    check_voice_scope(scope);
    
    voice_tables voices(get_self(), scope.value);
    auto vitr = voices.find(user.value);
    check(vitr != voices.end(), "user does not have voice");

    if (reduce) {
      check(amount <= vitr -> balance, "voice balance exceeded");
      percentage_used = amount / double(vitr -> balance);
    }
    voices.modify(vitr, _self, [&](auto & voice){
      if (reduce) {
        voice.balance -= amount;
      } else {
        voice.balance += amount;
      }
    });
  }
  return percentage_used;
}

void proposals::set_voice (name user, uint64_t amount, name scope) {
  if (scope == ""_n) {
    voice_tables voice_alliance(get_self(), alliance_type.value);

    auto vitr = voice.find(user.value);
    auto vaitr = voice_alliance.find(user.value);

    if (vitr == voice.end()) {
        voice.emplace(_self, [&](auto& voice) {
            voice.account = user;
            voice.balance = amount;
        });
        size_change("voice.sz"_n, 1);
    } else {
      voice.modify(vitr, _self, [&](auto& voice) {
        voice.balance = amount;
      });
    }

    if (vaitr == voice.end()) {
      voice_alliance.emplace(_self, [&](auto & voice){
        voice.account = user;
        voice.balance = amount;
      });
    } else {
      voice_alliance.modify(vaitr, _self, [&](auto & voice){
        voice.balance = amount;
      });
    }

  } else {
    check_voice_scope(scope);
    
    voice_tables voices(get_self(), scope.value);
    auto vitr = voices.find(user.value);
    check(vitr != voices.end(), "user does not have a voice entry");

    voices.modify(vitr, _self, [&](auto & voice){
      voice.balance = amount;
    });
  }
}

void proposals::erase_voice (name user) {
  require_auth(get_self());
  
  voice_tables voice_alliance(get_self(), alliance_type.value);
  
  auto vitr = voice.find(user.value);
  auto vaitr = voice_alliance.find(user.value);

  voice.erase(vitr);
  voice_alliance.erase(vaitr);

  size_change("voice.sz"_n, -1);
  
  auto aitr = actives.find(user.value);
  if (aitr != actives.end()) {
    actives.erase(aitr);
    size_change(user_active_size, -1);
  }

}

void proposals::changetrust(name user, bool trust) {
    require_auth(get_self());

    auto vitr = voice.find(user.value);

    if (vitr == voice.end() && trust) {
      recover_voice(user); // Issue 
      //set_voice(user, 0, ""_n);
    } else if (vitr != voice.end() && !trust) {
      erase_voice(user);
    }
}

void proposals::deposit(asset quantity) {
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
    auto reward_points = config_get(name("proppass.rep"));
    action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(beneficiary, reward_points)
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

void proposals::addactive(name account) {
  require_auth(get_self());

  auto aitr = actives.find(account.value);
  if (aitr == actives.end()) {
    actives.emplace(_self, [&](auto & a){
      a.account = account;
      a.timestamp = eosio::current_time_point().sec_since_epoch();
    });
    size_change(user_active_size, 1);
    recover_voice(account);
  }
}

uint64_t proposals::calculate_decay(uint64_t voice) {
  cycle_table c = cycle.get_or_create(get_self(), cycle_table());
  
  uint64_t decay_percentage = config_get(name("vdecayprntge"));
  uint64_t decay_time = config_get(name("decaytime"));
  uint64_t decay_sec = config_get(name("propdecaysec"));
  uint64_t temp = c.t_onperiod + decay_time;

  check(decay_percentage <= 100, "The decay percentage could not be grater than 100%");

  if (temp >= c.t_voicedecay) { return voice; }
  uint64_t n = ((c.t_voicedecay - temp) / decay_sec) + 1;
  
  double multiplier = 1.0 - (decay_percentage / 100.0);
  return voice * pow(multiplier, n);
}

void proposals::recover_voice(name account) {
  DEFINE_CS_POINTS_TABLE
  DEFINE_CS_POINTS_TABLE_MULTI_INDEX
  
  cs_points_tables cspoints(contracts::harvest, contracts::harvest.value);

  auto csitr = cspoints.find(account.value);
  uint64_t voice_amount = 0;

  if (csitr != cspoints.end()) {
    voice_amount = calculate_decay(csitr -> rank);
  }

  set_voice(account, voice_amount, ""_n);

  size_change(cycle_vote_power_size, voice_amount);

}

void proposals::size_change(name id, int64_t delta) {
  size_tables sizes(get_self(), get_self().value);

  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    check(delta >= 0, "can't add negagtive size");
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = delta;
    });
  } else {
    uint64_t newsize = sitr->size + delta; 
    if (delta < 0) {
      if (sitr->size < -delta) {
        newsize = 0;
      }
    }
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

void proposals::size_set(name id, int64_t value) {
  size_tables sizes(get_self(), get_self().value);

  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = value;
    });
  } else {
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = value;
    });
  }
}

void proposals::testvdecay(uint64_t timestamp) {
  require_auth(get_self());
  cycle_table c = cycle.get_or_create(get_self(), cycle_table());
  c.t_voicedecay = timestamp;
  cycle.set(c, get_self());
}

void proposals::testsetvoice(name user, uint64_t amount) {
  require_auth(get_self());
  set_voice(user, amount, ""_n);
}

name proposals::get_type (name fund) {
  if (fund == bankaccts::alliances) {
    return alliance_type;
  } else if (fund == bankaccts::campaigns || bankaccts::milestone) {
    return campaign_type;
  }
  return "none"_n;
}

ACTION proposals::initnumprop() {
  require_auth(get_self());

  uint64_t total_proposals = 0;

  auto pitr = props.rbegin();
  while(pitr != props.rend()) {
    if (pitr->status == status_open && pitr->stage == stage_active) {
      total_proposals++;
    }
    if (pitr->status != status_open) {
      break;
    }
    pitr++;
  }

  size_tables sizes(get_self(), get_self().value);
  auto sitr = sizes.find(prop_active_size.value);

  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = prop_active_size;
      item.size = total_proposals;
    });
  } else {
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = total_proposals;
    });
  }
}

void proposals::check_voice_scope (name scope) {
  check(scope == _self || scope == alliance_type, "invalid scope for voice");
}

bool proposals::is_trust_delegated (name account, name scope) {
  delegate_trust_tables deltrusts(get_self(), scope.value);
  auto ditr = deltrusts.find(account.value);
  return ditr != deltrusts.end();
}

ACTION proposals::delegate (name delegator, name delegatee, name scope) {

  require_auth(delegator);
  check_voice_scope(scope);

  voice_tables voice(get_self(), scope.value);
  auto vitr = voice.find(delegator.value);
  check(vitr != voice.end(), "delegatee does not have voice");

  delegate_trust_tables deltrusts(get_self(), scope.value);
  auto ditr = deltrusts.find(delegator.value);

  name current = delegatee;
  bool has_no_cycles = false;
  uint64_t max_depth = config_get("dlegate.dpth"_n);

  for (int i = 0; i < max_depth; i++) {
    auto dditr = deltrusts.find(current.value);
    if (dditr != deltrusts.end()) {
      current = dditr -> delegatee;
      if (current == delegator) {
        break;
      }
    } else {
      has_no_cycles = true;
      break;
    }
  }

  check(has_no_cycles, "can not add delegatee, cycles are not allowed");

  if (ditr != deltrusts.end()) {
    deltrusts.modify(ditr, _self, [&](auto & item){
      item.delegatee = delegatee;
      item.weight = 1.0;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  } else {
    deltrusts.emplace(_self, [&](auto & item){
      item.delegator = delegator;
      item.delegatee = delegatee;
      item.weight = 1.0;
      item.timestamp = eosio::current_time_point().sec_since_epoch();
    });
  }

}

void proposals::send_vote_on_behalf (name voter, uint64_t id, uint64_t amount, name option) {
  action vote_on_behalf_action(
    permission_level{get_self(), "active"_n},
    get_self(),
    "voteonbehalf"_n,
    std::make_tuple(voter, id, amount, option)
  );
  transaction tx;
  tx.actions.emplace_back(vote_on_behalf_action);
  // tx.delay_sec = 1;
  tx.send(voter.value, _self);
}

void proposals::send_mimic_delegatee_vote (name delegatee, name scope, uint64_t proposal_id, double percentage_used, name option) {

  uint64_t batch_size = config_get("batchsize"_n);

  delegate_trust_tables deltrusts(get_self(), scope.value);
  auto deltrusts_by_delegatee_delegator = deltrusts.get_index<"byddelegator"_n>();

  uint128_t id = uint128_t(delegatee.value) << 64;
  auto ditr = deltrusts_by_delegatee_delegator.lower_bound(id);

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr -> delegatee == delegatee) {
    action mimic_action(
      permission_level{get_self(), "active"_n},
      get_self(),
      "mimicvote"_n,
      std::make_tuple(delegatee, ditr -> delegator, scope, proposal_id, percentage_used, option, batch_size)
    );

    transaction tx;
    tx.actions.emplace_back(mimic_action);
    tx.delay_sec = 1;
    tx.send(delegatee.value + 1, _self);
  }

}

ACTION proposals::mimicvote (name delegatee, name delegator, name scope, uint64_t proposal_id, double percentage_used, name option, uint64_t chunksize) {

  require_auth(get_self());

  delegate_trust_tables deltrusts(get_self(), scope.value);
  auto deltrusts_by_delegatee_delegator = deltrusts.get_index<"byddelegator"_n>();

  check_voice_scope(scope);
  voice_tables voices(get_self(), scope.value);

  uint128_t id = (uint128_t(delegatee.value) << 64) + delegator.value;

  auto ditr = deltrusts_by_delegatee_delegator.find(id);
  uint64_t count = 0;

  while (ditr != deltrusts_by_delegatee_delegator.end() && ditr -> delegatee == delegatee && count < chunksize) {

    name voter = ditr -> delegator;

    auto vitr = voices.find(voter.value);
    if (option == trust) {
      send_vote_on_behalf(voter, proposal_id, vitr -> balance * percentage_used, trust);
    } else if (option == distrust) {
      send_vote_on_behalf(voter, proposal_id, vitr -> balance * percentage_used, distrust);
    } else if (option == abstain) {
      send_vote_on_behalf(voter, proposal_id, uint64_t(0), abstain);
    }

    ditr++;
    count++;
  }

  if (ditr != deltrusts_by_delegatee_delegator.end() && ditr -> delegatee == delegatee) {
    action next_execution(
      permission_level{get_self(), "active"_n},
      get_self(),
      "mimicvote"_n,
      std::make_tuple(delegatee, ditr -> delegator, scope, proposal_id, percentage_used, option, chunksize)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(delegatee.value + 1, _self);
  }

}

ACTION proposals::undelegate (name delegator, name scope) {
  check_voice_scope(scope);

  delegate_trust_tables deltrusts(get_self(), scope.value);
  auto ditr = deltrusts.find(delegator.value);

  check(ditr != deltrusts.end(), "delegator not found");

  if (!has_auth(ditr -> delegatee)) {
    require_auth(delegator);
  } else {
    require_auth(ditr -> delegatee);
  }

  deltrusts.erase(ditr);
}

void proposals::increase_voice_cast (name voter, uint64_t amount, name option) {

  cycle_table c = cycle.get();
  auto citr = cyclestats.find(c.propcycle);

  if (citr != cyclestats.end()) {
    cyclestats.modify(citr, _self, [&](auto & item){
      item.total_voice_cast += amount;
      if (option == trust) {
        item.total_favour += amount;
      } else if (option == distrust) {
        item.total_against += amount;
      }
      item.num_votes += 1;
    });
  }

}

uint64_t proposals::calc_quorum_base (uint64_t propcycle) {

  uint64_t num_cycles = config_get("prop.cyc.qb"_n);
  uint64_t total = 0;
  uint64_t count = 0;

  auto citr = cyclestats.find(propcycle);

  if (citr == cyclestats.end()) {
    // in case there is no information for this propcycle
    return get_size(user_active_size) * 50 / 2;
  }

  while (count < num_cycles) {

    total += citr -> total_voice_cast;
    // total += citr -> num_votes; // uncomment to make it count number of voters
    count++;

    if (citr == cyclestats.begin()) {
      break;
    } else {
      citr--;
    }

  }

  return count > 0 ? total / count : 0;

}

void proposals::add_voted_proposal (uint64_t proposal_id) {

  cycle_table c = cycle.get();
  voted_proposals_tables votedprops(get_self(), c.propcycle);

  auto vpitr = votedprops.find(proposal_id);

  if (vpitr == votedprops.end()) {
    votedprops.emplace(_self, [&](auto & prop){
      prop.proposal_id = proposal_id;
    });
  }

}

ACTION proposals::migrtevotedp () {
  require_auth(get_self());

  auto pitr = props.begin();
  
  while (pitr != props.end()) {
    if (pitr -> passed_cycle != 0) {
      voted_proposals_tables votedprops(get_self(), pitr -> passed_cycle);
      auto vpitr = votedprops.find(pitr -> id);
      if (vpitr == votedprops.end()) {
        votedprops.emplace(_self, [&](auto & item){
          item.proposal_id = pitr -> id;
        });
      }
    }
    pitr++;
  }

}

ACTION proposals::migrpass () {
  require_auth(get_self());
  // fix passed for rejected proposla in the range from cycle 25 

  // Manual review results: 

  // 72 - 25
  // 80 - 26 
  // 90 - 27
  // 95 - 28
  // 100 - 28 <-- current cycle

  auto pitr = props.find(72);

  while(pitr != props.end() && pitr->id <= 100) {
    if (pitr->status == status_rejected && pitr->passed_cycle == 0) {

      uint64_t cycle = 25;

      if (pitr->id >= 80) {
        cycle = 26;
      }

      if (pitr->id >= 90) {
        cycle = 27;
      }
      
      if (pitr->id >= 95) {
        cycle = 28;
      }

      props.modify(pitr, _self, [&](auto& proposal) {
        proposal.passed_cycle = cycle;
      });

    }
    pitr++;
  }
}

ACTION proposals::migstats (uint64_t cycle) {
  require_auth(get_self());

  auto citr = cyclestats.find(cycle);
  while(citr != cyclestats.end()) {
    citr = cyclestats.erase(citr);
  }

  // calculate vote power for cycle
  auto pitr = props.find(72);

  uint64_t num_proposals = 0;
  uint64_t num_votes = 0;
  uint64_t total_voice_cast = 0;
  uint64_t total_favour = 0;
  uint64_t total_against = 0; 

  while(pitr != props.end() && pitr->passed_cycle <= cycle) {
    if (pitr->passed_cycle == cycle) {
      print("passed: "+std::to_string(cycle) + " " + std::to_string(pitr->id));
      num_proposals++;
      votes_tables votes(get_self(), pitr->id);
      auto vitr = votes.begin();
      while(vitr != votes.end()) {
        num_votes++;
        total_voice_cast += vitr -> amount;
        if (vitr->favour) {
          total_favour += vitr -> amount;
        } else {
          total_against += vitr -> amount;
        }
        vitr++;
      }
    }
    pitr++;
  }

  cyclestats.emplace(_self, [&](auto & item){
    item.propcycle = cycle;
    item.num_proposals = num_proposals;
    item.num_votes = num_votes;
    item.total_voice_cast = total_voice_cast;
    item.total_favour = total_favour;
    item.total_against = total_against;
  });

}


void proposals::migcycstat() {
  require_auth(get_self());

  cycle_table c = cycle.get();

  uint64_t quorum_vote_base = calc_quorum_base(c.propcycle - 1);

  auto citr = cyclestats.find(c.propcycle);

  uint64_t num_proposals = citr->active_props.size();

  cyclestats.modify(citr, _self, [&](auto & item){
    item.num_proposals = num_proposals;
    item.quorum_vote_base = quorum_vote_base;
    item.quorum_votes_needed = quorum_vote_base * (get_quorum(num_proposals) / 100.0);
    item.unity_needed = double(config_get("propmajority"_n)) / 100.0;
  });

}

void proposals::testpropquor(uint64_t current_cycle, uint64_t prop_id) {
  require_auth(get_self());
  auto pitr = props.find(prop_id);
  check(pitr != props.end(), "proposal not found");

  uint64_t votes_in_favor = pitr->favour; // only votes in favor are counted
  auto citr = cyclestats.find(current_cycle);
  uint64_t quorum_votes_needed = citr != cyclestats.end() ? citr->quorum_votes_needed : 0;
  bool valid_quorum = votes_in_favor >= quorum_votes_needed;

  print(
    " vp favor " + std::to_string(votes_in_favor) + " " +
    "needed: " + std::to_string(quorum_votes_needed) + " " + 
    "valid: " + ( valid_quorum ? "YES " : "NO ") 
  );

}