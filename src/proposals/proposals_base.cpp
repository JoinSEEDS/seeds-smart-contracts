#include <proposals/proposals_base.hpp>


void Proposal::create (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = proposals_t.available_primary_key();

  name creator = std::get<name>(args["creator"]);
  name fund = std::get<name>(args["fund"]);

  proposals_t.emplace(contract_name, [&](auto & item) {
    item.proposal_id = proposal_id;
    item.favour = 0;
    item.against = 0;
    item.staked = asset(0, utils::seeds_symbol);
    item.creator = creator;
    item.title = std::get<string>(args["title"]);
    item.summary = std::get<string>(args["summary"]);
    item.description = std::get<string>(args["description"]);
    item.image = std::get<string>(args["image"]);
    item.url = std::get<string>(args["url"]);
    item.created_at = current_time_point();
    item.status = ProposalsCommon::status_open;
    item.stage = ProposalsCommon::stage_staged;
    item.type = std::get<name>(args["type"]);
    item.last_ran_cycle = 0;
    item.age = 0;
    item.fund = fund;
    item.quantity = std::get<asset>(args["quantity"]);
  });

  args.insert(std::make_tuple("proposal_id", proposal_id));
  create_impl(args);

}

void Proposal::update (std::map<std::string, VariantValue> & args) {
  name contract_name = this->m_contract.get_self();
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  check(pitr->stage == ProposalsCommon::stage_staged, "can not update proposal, it is not staged");

  this->m_contract.check_attributes(args);

  proposals_t.modify(pitr, contract_name, [&](auto & item) {
    item.title = std::get<string>(args["title"]);
    item.summary = std::get<string>(args["summary"]);
    item.description = std::get<string>(args["description"]);
    item.image = std::get<string>(args["image"]);
    item.url = std::get<string>(args["url"]);
  });

  update_impl(args);
}

void Proposal::cancel (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.find(proposal_id);

  check(pitr->stage == ProposalsCommon::stage_staged, "can not cancel proposal, it is not staged");

  proposals_t.erase(pitr);

  if (paitr != propaux_t.end()) {
    propaux_t.erase(paitr); 
  }

  cancel_impl(args);

}

void Proposal::evaluate (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  name current_stage = pitr->stage;
  name current_status = pitr->status;

  uint64_t number_active_proposals = 0;
  uint64_t quorum_votes_needed = 0;

  name prop_type = this->m_contract.get_fund_type(pitr->fund);

  dao::support_level_tables support_t(contract_name, prop_type.value);
  auto citr = support_t.find(propcycle);

  if (citr !=  support_t.end()) {
    number_active_proposals = citr->num_proposals;
    quorum_votes_needed = citr->voice_needed;
  } else {
    number_active_proposals = this->m_contract.get_size(this->m_contract.prop_active_size);
  }

  if (current_stage == ProposalsCommon::stage_active) {
    
    std::map<std::string, VariantValue> propm = { { "favour", pitr->favour }, { "against", pitr->against } };
    bool passed = check_prop_majority(propm);

    bool valid_quorum = false;

    if (pitr->status == ProposalsCommon::status_evaluate) { // in evaluate status, we only check unity. 
      valid_quorum = true;
    } else { // in open status, quorum is calculated
      uint64_t votes_in_favor = pitr->favour; // only votes in favor are counted
      valid_quorum = votes_in_favor >= quorum_votes_needed;
    }

    if (passed && valid_quorum) {

      if (current_status == ProposalsCommon::status_open) {

        this->m_contract.send_inline_action(
          permission_level(contracts::bank, "active"_n),
          contracts::token,
          "transfer"_n,
          std::make_tuple(contracts::bank, pitr->creator, pitr->quantity, string(""))
        );

        uint64_t reward_points = this->m_contract.config_get(name("proppass.rep"));

        this->m_contract.send_inline_action(
          permission_level(contracts::accounts, "addrep"_n),
          contracts::accounts,
          "addrep"_n,
          std::make_tuple(pitr->creator, reward_points)
        );

        status_open_impl(args);

      } else { // evaluate status
        status_eval_impl(args);
      }

    }
    else {

      if (current_status != ProposalsCommon::status_evaluate) {
        this->m_contract.send_inline_action(
          permission_level(contracts::bank, "active"_n),
          contracts::token,
          "burn"_n,
          std::make_tuple(contracts::bank, pitr->staked)
        );
      } else {
        this->m_contract.send_inline_action(
          permission_level(contracts::accounts, "active"_n),
          contracts::accounts,
          "punish"_n,
          std::make_tuple(pitr->creator, this->m_contract.config_get("prop.evl.psh"_n))
        );
        
        status_rejected_impl(args);
      }

      proposals_t.modify(pitr, contract_name, [&](auto& proposal) {
        proposal.staked = asset(0, utils::seeds_symbol);
        proposal.status = ProposalsCommon::status_rejected;
        proposal.stage = ProposalsCommon::stage_done;
      });

      propaux_t.modify(paitr, contract_name, [&](auto & propaux){
        if (current_status != ProposalsCommon::status_evaluate) {
          propaux.special_attributes.at("passed_cycle") = propcycle;
        }
      });

    }

    this->m_contract.size_change(this->m_contract.prop_active_size, -1);


  } else if (current_stage == ProposalsCommon::stage_staged) {
    uint64_t m_stake = this->m_contract.min_stake(pitr->quantity, pitr->fund);

    if (pitr->staked.amount >= m_stake) {
      proposals_t.modify(pitr, contract_name, [&](auto& proposal) {
        proposal.stage = ProposalsCommon::stage_active;
      });
      this->m_contract.size_change(this->m_contract.prop_active_size, 1);
      this->m_contract.update_cycle_stats_from_proposal(pitr->proposal_id, prop_type, ProposalsCommon::stage_active);
    }
  }

}

bool Proposal::check_prop_majority (std::map<std::string, VariantValue> & args) {
  uint64_t favour = std::get<uint64_t>(args["favour"]);
  uint64_t against = std::get<uint64_t>(args["against"]);

  uint64_t prop_majority = this->m_contract.config_get(name("propmajority"));
  double majority = double(prop_majority) / 100.0;
  double fav = double(favour);
  return favour > 0 && fav >= double(favour + against) * majority;
}

void Proposal::check_can_vote (const name & status, const name & stage) {
  check(status == ProposalsCommon::status_open, "can not vote, proposal is not in open status");
  check(stage == ProposalsCommon::stage_active, "can not vote, proposal is not in active stage");
}

void Proposal::create_impl(std::map<std::string, VariantValue> & args) {}

void Proposal::update_impl(std::map<std::string, VariantValue> & args) {}

void Proposal::cancel_impl(std::map<std::string, VariantValue> & args) {}

void Proposal::status_open_impl (std::map<std::string, VariantValue> & args) {}

void Proposal::status_eval_impl (std::map<std::string, VariantValue> & args) {}

void Proposal::status_rejected_impl (std::map<std::string, VariantValue> & args) {}

void Proposal::callback (std::map<std::string, VariantValue> & args) {}
