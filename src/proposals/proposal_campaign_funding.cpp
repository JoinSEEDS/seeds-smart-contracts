#include <proposals/proposal_campaign_funding.hpp>


void ProposalCampaignFunding::create_impl (std::map<std::string, VariantValue> & args) {

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  asset quantity = std::get<asset>(args["quantity"]);
  utils::check_asset(quantity);
  check(quantity.amount > 0, "quantity amount must be greater than zero");

  name creator = std::get<name>(args["creator"]);
  this->m_contract.check_citizen(creator);

  name fund_type = this->m_contract.get_fund_type(std::get<name>(args["fund"]));
  check(fund_type == this->m_contract.campaign_fund, "fund must be of type: " + this->m_contract.campaign_fund.to_string());

  name recipient = std::get<name>(args["recipient"]);
  check(is_account(recipient), "recipient is not a valid account: " + recipient.to_string());

  string pay_percentages = "25,25,25,25";

  auto pay_percentages_itr = args.find("pay_percentages");
  if (pay_percentages_itr != args.end()) {
    pay_percentages = std::get<string>(pay_percentages_itr->second);
    check_percentages(*(values_to_vector(pay_percentages)));
  }

  propaux_t.emplace(contract_name, [&](auto & item){
    item.proposal_id = std::get<uint64_t>(args["proposal_id"]);
    item.special_attributes.insert(std::make_pair("pay_percentages", pay_percentages));
    item.special_attributes.insert(std::make_pair("recipient", recipient));
    item.special_attributes.insert(std::make_pair("current_payout", asset(0, utils::seeds_symbol)));
    item.special_attributes.insert(std::make_pair("executed", false));
    item.special_attributes.insert(std::make_pair("passed_cycle", uint64_t(0)));
  });

}

void ProposalCampaignFunding::update_impl (std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");
  
  string pay_percentages = "25,25,25,25";

  auto pay_percentages_itr = args.find("pay_percentages");
  if (pay_percentages_itr != args.end()) {
    pay_percentages = std::get<string>(pay_percentages_itr->second);
    check_percentages(*(values_to_vector(pay_percentages)));
  }

  propaux_t.modify(paitr, contract_name, [&](auto & item){
    item.special_attributes.at("pay_percentages") = pay_percentages;
  });

}

void ProposalCampaignFunding::status_open_impl (std::map<std::string, VariantValue> & args) {

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);  

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  asset current_payout = std::get<asset>(paitr->special_attributes.at("current_payout"));
  string pay_percentages = std::get<string>(paitr->special_attributes.at("pay_percentages"));
  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));

  asset payout_amount = get_payout_amount(*(values_to_vector(pay_percentages)), 0, pitr->quantity, current_payout);

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, recipient, payout_amount, string(""))
  );

  proposals_t.modify(pitr, contract_name, [&](auto & item){
    item.age = 0;
    item.staked = asset(0, utils::seeds_symbol);
    item.status = ProposalsCommon::status_evaluate;
    item.last_ran_cycle = propcycle;
  });

  propaux_t.modify(paitr, contract_name, [&](auto & item){
    item.special_attributes.at("current_payout") = current_payout + payout_amount;
    item.special_attributes.at("passed_cycle") = propcycle;
  });

  name prop_type = this->m_contract.get_fund_type(pitr->fund);
  this->m_contract.update_cycle_stats_from_proposal(proposal_id, prop_type, ProposalsCommon::status_evaluate);

}

void ProposalCampaignFunding::status_eval_impl (std::map<std::string, VariantValue> & args) {

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  asset current_payout = std::get<asset>(paitr->special_attributes.at("current_payout"));
  string pay_percentages = std::get<string>(paitr->special_attributes.at("pay_percentages"));
  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));

  uint64_t age = pitr->age + 1;
  std::vector<uint64_t> & pay_per = *(values_to_vector(pay_percentages));

  asset payout_amount = get_payout_amount(pay_per, age, pitr->quantity, current_payout);

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, recipient, payout_amount, string(""))
  );

  uint64_t num_cycles = pay_per.size() - 1;

  proposals_t.modify(pitr, contract_name, [&](auto & item){
    item.age = age;
    if (age >= num_cycles) {
      item.status = ProposalsCommon::status_passed;
      item.stage = ProposalsCommon::stage_done;
    } else {
      name prop_type = this->m_contract.get_fund_type(pitr->fund);
      this->m_contract.update_cycle_stats_from_proposal(proposal_id, prop_type, ProposalsCommon::status_evaluate);
    }
    item.last_ran_cycle = propcycle;
  });

  propaux_t.modify(paitr, contract_name, [&](auto & item){
    if (age >= num_cycles) {
      item.special_attributes.at("executed") = true;
    }
    item.special_attributes.at("current_payout") = current_payout + payout_amount;
  });

}

name ProposalCampaignFunding::get_scope () {
  return this->m_contract.campaign_scope;
}

name ProposalCampaignFunding::get_fund_type () {
  return this->m_contract.campaign_fund;
}

std::vector<uint64_t> * ProposalCampaignFunding::values_to_vector (const string & values) {

  std::vector<uint64_t> * v = new std::vector<uint64_t>;

  auto it = std::begin(values);
  while (true) {
    auto commaPosition = std::find(it, std::end(values), ',');
    v->push_back(std::stoi(string(it, commaPosition)));
    if (commaPosition == std::end(values)) {
      break;
    }
    it = std::next(commaPosition);
  }

  return v;

}

void ProposalCampaignFunding::check_percentages (std::vector<uint64_t> & pay_percentages) {
  
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

asset ProposalCampaignFunding::get_payout_amount (
  std::vector<uint64_t> & pay_percentages, 
  const uint64_t & age, 
  const asset & total_amount, 
  const asset & current_payout
) {

  if (age >= pay_percentages.size()) { return asset(0, utils::seeds_symbol); }

  if (age == pay_percentages.size() - 1) {
    return total_amount - current_payout;
  }

  double payout_percentage = pay_percentages[age] / 100.0;
  uint64_t payout_amount = payout_percentage * total_amount.amount;
  
  return asset(payout_amount, utils::seeds_symbol);

}
