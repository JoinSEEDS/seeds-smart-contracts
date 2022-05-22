#include <proposals/proposal_milestone.hpp>

void ProposalMilestone::create_impl (std::map<std::string, VariantValue> & args) {

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  asset quantity = std::get<asset>(args["quantity"]);
  utils::check_asset(quantity);
  check(quantity.amount > 0, "quantity amount must be greater than zero");

  name creator = std::get<name>(args["creator"]);
  name fund_type = this->m_contract.get_fund_type(std::get<name>(args["fund"]));
  check(fund_type == this->m_contract.milestone_fund, "fund must be of type: " + this->m_contract.milestone_fund.to_string());

  this->m_contract.check_citizen(creator);

  name recipient = std::get<name>(args["recipient"]);
  check(recipient  == bankaccts::hyphabank, 
    "Hypha milestone proposals must go to " + bankaccts::hyphabank.to_string() + " - wrong recepient" + recipient.to_string());

  propaux_t.emplace(contract_name, [&](auto & item){
    item.proposal_id = std::get<uint64_t>(args["proposal_id"]);
    item.special_attributes.insert(std::make_pair("recipient", std::get<name>(args["recipient"])));
    item.special_attributes.insert(std::make_pair("current_payout", asset(0, utils::seeds_symbol)));
    item.special_attributes.insert(std::make_pair("executed", false));
    item.special_attributes.insert(std::make_pair("passed_cycle", uint64_t(0)));
  });

}

void ProposalMilestone::status_open_impl(std::map<std::string, VariantValue> & args) {

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);  

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));
  asset payout_amount = pitr->quantity;

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, recipient, payout_amount, string(""))
  );

  proposals_t.modify(pitr, contract_name, [&](auto & item){
    item.age = 0;
    item.staked = asset(0, utils::seeds_symbol);
    item.status = ProposalsCommon::status_passed;
    item.stage = ProposalsCommon::stage_done;
    item.last_ran_cycle = propcycle;
  });

  propaux_t.modify(paitr, contract_name, [&](auto & item){
    item.special_attributes.at("executed") = true;
    item.special_attributes.at("current_payout") = payout_amount;
    item.special_attributes.at("passed_cycle") = propcycle;
  });
  
}

name ProposalMilestone::get_scope () {
  return this->m_contract.milestone_scope;
}

name ProposalMilestone::get_fund_type () {
  return this->m_contract.milestone_fund;
}
