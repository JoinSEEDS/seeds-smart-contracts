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

  name fund_type = this->m_contract.get_fund_type(std::get<name>(args["fund"]));
  check(fund_type == this->m_contract.campaign_fund, "fund must be of type: " + this->m_contract.campaign_fund.to_string());

  uint64_t min_planted = this->m_contract.config_get("inv.min.plnt"_n);
  check(planted.amount >= min_planted, "the planted amount must be greater or equal than " + std::to_string(min_planted));

  uint64_t max_reward = this->m_contract.config_get("inv.max.rwrd"_n);
  check(reward.amount <= max_reward, "the reward can not be greater than " + std::to_string(max_reward));
  
  name contract_name = this->m_contract.get_self();
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  propaux_t.emplace(contract_name, [&](auto & item) {
    item.proposal_id = std::get<uint64_t>(args["proposal_id"]);
    item.special_attributes.insert(std::make_pair("current_payout", asset(0, utils::seeds_symbol)));
    item.special_attributes.insert(std::make_pair("passed_cycle", uint64_t(0)));
    item.special_attributes.insert(std::make_pair("lock_id", uint64_t(0)));
    item.special_attributes.insert(std::make_pair("max_age", uint64_t(6)));
    item.special_attributes.insert(std::make_pair("max_amount_per_invite", max_amount_per_invite));
    item.special_attributes.insert(std::make_pair("planted", planted));
    item.special_attributes.insert(std::make_pair("reward", reward));
    item.special_attributes.insert(std::make_pair("recipient", std::get<name>(args["recipient"])));
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
    item.special_attributes.at("recipient") = std::get<name>(args["recipient"]);
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
    std::make_tuple(pitr->fund, contract_name, payout_amount, string("invites"))
  );

  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, contracts::onboarding, payout_amount, "sponsor " + contract_name.to_string())
  );

  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));
  asset max_amount_per_invite = std::get<asset>(paitr->special_attributes.at("max_amount_per_invite"));
  asset planted = std::get<asset>(paitr->special_attributes.at("planted"));
  asset reward = std::get<asset>(paitr->special_attributes.at("reward"));

  this->m_contract.send_inline_action(
    permission_level(contracts::onboarding, "active"_n),
    contracts::onboarding,
    "createcmpdao"_n,
    std::make_tuple(
      contract_name,
      pitr->creator,
      max_amount_per_invite,
      planted,
      recipient,
      reward,
      payout_amount,
      proposal_id
    )
  );
}

void ProposalCampaignInvite::status_eval_impl(std::map<std::string, VariantValue> & args) {
  name contract_name = this->m_contract.get_self();

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  auto pitr = proposals_t.require_find(proposal_id, "proposal not found");
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  name prop_type = this->m_contract.get_fund_type(pitr->fund);

  asset payout_amount = asset(0, utils::seeds_symbol);

  uint64_t pay_perc_size = std::get<uint64_t>(paitr->special_attributes.at("max_age"));
  asset current_payout = std::get<asset>(paitr->special_attributes.at("current_payout"));

  if (pitr->age == 0) {
    payout_amount = pitr->quantity;
  }

  name recipient = std::get<name>(paitr->special_attributes.at("recipient"));
  this->m_contract.send_inline_action(
    permission_level(pitr->fund, "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(pitr->fund, recipient, payout_amount, "")
  );

  uint64_t num_cycles = pay_perc_size - 1;

  proposals_t.modify(pitr, contract_name, [&](auto & proposal) {
    proposal.age += 1;
    if (pitr->age >= num_cycles) {
      proposal.status = ProposalsCommon::status_passed;
      proposal.stage = ProposalsCommon::stage_done;
    } else {
      this->m_contract.update_cycle_stats_from_proposal(pitr->proposal_id, prop_type, ProposalsCommon::status_evaluate);
    }
  });

  propaux_t.modify(paitr, contract_name, [&](auto & proposal_aux) {
    proposal_aux.special_attributes.at("current_payout") = current_payout + payout_amount;
    if (pitr->age >= num_cycles) {
      proposal_aux.special_attributes.insert(std::make_pair("executed", true));
    }
  });
}


void ProposalCampaignInvite::status_rejected_impl(std::map<std::string, VariantValue> & args) {
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  name contract_name = this->m_contract.get_self();

  this->m_contract.send_inline_action(
    permission_level(contract_name, "active"_n),
    contracts::onboarding,
    "returnfunds"_n,
    std::make_tuple(proposal_id)
  );

  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);
  auto paitr = propaux_t.require_find(proposal_id, "proposal aux entry not found");

  propaux_t.modify(paitr, contract_name, [&](auto & proposal_aux) {
    proposal_aux.special_attributes.insert(std::make_pair("executed", true));
  });
}

void ProposalCampaignInvite::callback(std::map<std::string, VariantValue> & args) {
  require_auth(contract_name);

  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  dao::proposal_tables proposals_t(contract_name, contract_name.value);

  auto pitr = proposals_t.find(proposal_id);
  
  if (pitr == proposals_t.end()) { return; }

  proposals_t.modify(pitr, contract_name, [&](auto & item){
    item.proposal_id = proposal_id;
  });
}

name ProposalCampaignInvite::get_scope () {
  return this->m_contract.campaign_scope;
}

name ProposalCampaignInvite::get_fund_type () {
  return this->m_contract.campaign_fund;
}