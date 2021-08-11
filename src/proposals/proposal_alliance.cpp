#include <proposals/proposal_alliance.hpp>


void ProposalAlliance::create_impl (std::map<std::string, VariantValue> & args) {

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);
  dao::user_tables users_t(contracts::accounts, contracts::accounts.value);

  name creator = std::get<name>(args["creator"]);
  name fund_type = this->m_contract.get_fund_type(std::get<name>(args["fund"]));
  check(fund_type == this->m_contract.alliance_fund, "fund must be of type: " + this->m_contract.alliance_fund.to_string());

  auto uitr = users_t.require_find(creator.value, "no user");
  check(
    uitr->status == name("citizen") || 
    uitr->status == name("resident") ||
    uitr->type == "organisation"_n, 
    "user is not a resident or citizen or an organization with alliance proposal");

  propaux_t.emplace(contract_name, [&](auto & item){
    item.proposal_id = std::get<uint64_t>(args["proposal_id"]);
    item.special_attributes.insert(std::make_pair("current_payout", asset(0, utils::seeds_symbol)));
    item.special_attributes.insert(std::make_pair("passed_cycle", uint64_t(0)));
    item.special_attributes.insert(std::make_pair("recipient", std::get<name>(args["recipient"])));
    item.special_attributes.insert(std::make_pair("lock_id", uint64_t(0)));
  });

}

void ProposalAlliance::update_impl (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  check(pitr->type == ProposalsCommon::type_prop_alliance, "proposal has to be of type alliance");

}

void ProposalAlliance::status_open_impl(std::map<std::string, VariantValue> & args) {

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);  

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  name prop_type = this->m_contract.get_fund_type(pitr->fund);

  asset payout_amount = pitr->quantity;
  string memo = "proposal id: " + std::to_string(proposal_id);

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, contracts::escrow, payout_amount, memo)
  );

  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::escrow,
    "lock"_n,
    std::make_tuple(
      "event"_n,
      pitr->fund,
      recipient,
      pitr->quantity,
      "golive"_n,
      "dao.hypha"_n,
      time_point(current_time_point().time_since_epoch() + current_time_point().time_since_epoch()),
      memo
    )
  );

  proposals_t.modify(pitr, contract_name, [&](auto & proposal){
    proposal.age = 0;
    proposal.staked = asset(0, utils::seeds_symbol);
    proposal.last_ran_cycle = propcycle;
    proposal.status = ProposalsCommon::status_evaluate;
  });
  
  auto current_payout = paitr->special_attributes.find("current_payout")->second;

  propaux_t.modify(paitr, contract_name, [&](auto & propaux){
    propaux.special_attributes.at("current_payout") = std::get<asset>(current_payout) + payout_amount;
    propaux.special_attributes.at("passed_cycle") = propcycle;
  });

  this->m_contract.update_cycle_stats_from_proposal(proposal_id, prop_type, ProposalsCommon::status_evaluate);

}


void ProposalAlliance::status_eval_impl(std::map<std::string, VariantValue> & args) {

  dao::proposal_tables proposals_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");

  name prop_type = this->m_contract.get_fund_type(pitr->fund);

  proposals_t.modify(pitr, contract_name, [&](auto & proposal){
    proposal.age += 1;
    proposal.staked = asset(0, utils::seeds_symbol);
    proposal.last_ran_cycle = propcycle;
    proposal.status = ProposalsCommon::status_evaluate;
  });

  this->m_contract.update_cycle_stats_from_proposal(proposal_id, prop_type, ProposalsCommon::status_evaluate);

}


void ProposalAlliance::status_rejected_impl(std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  
  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);
  
  auto pitr = proposals_t.find(proposal_id);
  auto paitr = propaux_t.find(proposal_id);

  action(
    permission_level(pitr->fund, "active"_n),
    contracts::escrow,
    "cancellock"_n,
    std::make_tuple(std::get<uint64_t>(paitr->special_attributes.at("lock_id")))
  ).send();

  action(
    permission_level(pitr->fund, "active"_n),
    contracts::escrow,
    "withdraw"_n,
    std::make_tuple(pitr->fund, pitr->quantity)
  ).send();

}


name ProposalAlliance::get_scope () {
  return this->m_contract.alliance_scope;
}

name ProposalAlliance::get_fund_type () {
  return this->m_contract.alliance_fund;
}

