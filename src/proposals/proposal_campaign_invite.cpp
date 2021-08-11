#include <proposals/proposal_campaign_invite.hpp>
#include <eosio/system.hpp>

void ProposalCampaignInvite::create_impl (std::map<std::string, VariantValue> & args) {
  name creator = std::get<name>(args["creator"]);
  require_auth(creator);

  this->m_contract.check_citizen(creator);
  
  asset max_amount_per_invite = std::get<asset>(args["max_amount_per_invite"]);
  asset planted = std::get<asset>(args["planted"]);
  asset reward = std::get<asset>(args["reward"]);

  utils::check_asset(max_amount_per_invite);
  utils::check_asset(planted);
  utils::check_asset(reward);

  name contract_name = this->m_contract.get_self();
  name fund_type = this->m_contract.get_fund_type(std::get<name>(args["fund"]));

  print("----- FOUNDS -----", fund_type, " --- ",std::get<name>(args["fund"]));

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  check(fund_type == this->m_contract.campaign_fund, "fund must be of type: " + this->m_contract.campaign_fund.to_string());

  uint64_t min_planted = this->m_contract.config_get("inv.min.plnt"_n);
  check(planted.amount >= min_planted, "the planted amount must be greater or equal than " + std::to_string(min_planted));

  uint64_t max_reward = this->m_contract.config_get("inv.max.rwrd"_n);
  check(reward.amount <= max_reward, "the reward can not be greater than " + std::to_string(max_reward));

  propaux_t.emplace(contract_name, [&](auto & item) {
    item.proposal_id = std::get<uint64_t>(args["proposal_id"]);
    item.special_attributes.insert(std::make_pair("current_payout", asset(0, utils::seeds_symbol)));
    item.special_attributes.insert(std::make_pair("passed_cycle", uint64_t(0)));
    item.special_attributes.insert(std::make_pair("lock_id", uint64_t(0)));
    item.special_attributes.insert(std::make_pair("max_age", uint64_t(6)));
    item.special_attributes.insert(std::make_pair("max_amount_per_invite", max_amount_per_invite));
    item.special_attributes.insert(std::make_pair("planted", planted));
    item.special_attributes.insert(std::make_pair("reward", reward));
  });
}

void ProposalCampaignInvite::update_impl (std::map<std::string, VariantValue> & args) {
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  name contract_name = this->m_contract.get_self();
  dao::proposal_tables proposals_t(contract_name, contract_name.value);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  check(pitr->type == ProposalsCommon::type_prop_campaign_invite, "proposal has to be of type campaign");

  asset max_amount_per_invite = std::get<asset>(args["max_amount_per_invite"]);
  asset planted = std::get<asset>(args["planted"]);
  asset reward = std::get<asset>(args["reward"]);

  utils::check_asset(max_amount_per_invite);
  utils::check_asset(planted);
  utils::check_asset(reward);

  uint64_t min_planted = this->m_contract.config_get("inv.min.plnt"_n);
  check(planted.amount >= min_planted, "the planted amount must be greater or equal than " + std::to_string(min_planted));

  uint64_t max_reward = this->m_contract.config_get("inv.max.rwrd"_n);
  check(reward.amount <= max_reward, "the reward can not be greater than " + std::to_string(max_reward));

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux not found");

  propaux_t.modify(paitr, contract_name, [&](auto & item) {
    item.special_attributes.at("current_payout") = std::get<asset>(args["current_payout"]);
    item.special_attributes.at("passed_cycle") = std::get<uint64_t>(args["passed_cycle"]);
    item.special_attributes.at("lock_id") = std::get<uint64_t>(args["lock_id"]);
    item.special_attributes.at("max_age") = std::get<uint64_t>(args["max_age"]);
    item.special_attributes.at("max_amount_per_invite") = max_amount_per_invite;
    item.special_attributes.at("planted") = planted;
    item.special_attributes.at("reward") = reward;
  });
}

void ProposalCampaignInvite::status_open_impl(std::map<std::string, VariantValue> & args) {
  name contract_name = this->m_contract.get_self();
  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  name prop_type = this->m_contract.get_fund_type(pitr->fund);

  asset payout_amount = pitr->quantity;
  if (payout_amount.amount == 0) return;

  utils::check_asset(payout_amount);

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, contract_name.to_string(), payout_amount, "invites")
  );

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, contracts::onboarding, payout_amount, "sponsor " + contract_name.to_string())
  );

  this->m_contract.send_inline_action(
    permission_level(contract_name, "active"_n),
    contracts::onboarding,
    "createcmpdao"_n,
    std::make_tuple(
      contract_name.to_string(),
      pitr->creator,
      // paitr->max_amount_per_invite,
      // paitr->planted,
      // paitr->recipient,
      // paitr->reward,
      paitr->special_attributes.at("max_amount_per_invite"),
      paitr->special_attributes.at("planted"),
      pitr->recipient,
      paitr->special_attributes.at("reward"),
      payout_amount,
      pitr->proposal_id
    )
  );
  std::make_tuple(origin_account, owner, max_amount_per_invite, planted, reward_owner, reward, total_amount, proposal_id)

  // action(
  //   permission_level(get_self(), "active"_n),
  //   contracts::onboarding,
  //   "createcampg"_n,
  //   std::make_tuple(origin_account, owner, max_amount_per_invite, planted, reward_owner, reward, total_amount, proposal_id)
  // ).send();
}


void ProposalCampaignInvite::status_eval_impl(std::map<std::string, VariantValue> & args) {
  name contract_name = this->m_contract.get_self();
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


void ProposalCampaignInvite::status_rejected_impl(std::map<std::string, VariantValue> & args) {

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  name contract_name = this->m_contract.get_self();
  
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


name ProposalCampaignInvite::get_scope () {
  return this->m_contract.campaign_scope;
}

name ProposalCampaignInvite::get_fund_type () {
  return this->m_contract.campaign_fund;
}