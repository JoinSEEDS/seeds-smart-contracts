#include <proposals/referendum_settings.hpp>

void ReferendumSettings::create (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  name setting_name = std::get<name>(args["setting_name"]);

  std::unique_ptr<SettingInfo> s_info = std::unique_ptr<SettingInfo>(get_setting_info(setting_name));

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  uint64_t proposal_id = proposals_t.available_primary_key();

  uint64_t min_test_cycles = this->m_contract.config_get("refmintest"_n);
  uint64_t test_cycles = std::get<uint64_t>(args["test_cycles"]);
  check(test_cycles >= min_test_cycles, "the number of test cycles must be at least " + std::to_string(min_test_cycles));

  uint64_t min_eval_cycles = this->m_contract.config_get("refmineval"_n);
  uint64_t eval_cycles = std::get<uint64_t>(args["eval_cycles"]);
  check(eval_cycles >= min_eval_cycles, "the number of eval cycles must be at least " + std::to_string(min_eval_cycles));

  proposals_t.emplace(contract_name, [&](auto & item) {
    item.proposal_id = proposal_id;
    item.favour = 0;
    item.against = 0;
    item.staked = asset(0, utils::seeds_symbol);
    item.creator = std::get<name>(args["creator"]);
    item.title = std::get<string>(args["title"]);
    item.summary = std::get<string>(args["summary"]);
    item.description = std::get<string>(args["description"]);
    item.image = std::get<string>(args["image"]);
    item.url = std::get<string>(args["url"]);
    item.created_at = current_time_point();
    item.status = ProposalsCommon::status_open;
    item.stage = ProposalsCommon::stage_staged;
    item.type = ProposalsCommon::type_ref_setting;
    item.last_ran_cycle = 0;
    item.age = 0;
  });

  propaux_t.emplace(contract_name, [&](auto & item){
    item.proposal_id = proposal_id;
    item.special_attributes.insert(std::make_pair("setting_name", setting_name));
    item.special_attributes.insert(std::make_pair("is_float", s_info->is_float));
    if (s_info->is_float) {
      item.special_attributes.insert(std::make_pair("new_value", std::get<double>(args["new_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", s_info->previous_value_double));
    } else {
      item.special_attributes.insert(std::make_pair("new_value", std::get<uint64_t>(args["new_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", s_info->previous_value_uint));
    }
    item.special_attributes.insert(std::make_pair("cycles_per_status", "1," + std::to_string(test_cycles) + "," + std::to_string(eval_cycles)));
  });

}

void ReferendumSettings::update (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  name setting_name = std::get<name>(args["setting_name"]);
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  std::unique_ptr<SettingInfo> s_info = std::unique_ptr<SettingInfo>(get_setting_info(setting_name));

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  auto ritr = proposals_t.require_find(proposal_id, "referendum not found");
  auto raitr = propaux_t.require_find(proposal_id, "refaux entry not found");

  check(ritr->stage == ProposalsCommon::stage_staged, "can not update referendum, it is not staged");
  check(ritr->type == ProposalsCommon::type_ref_setting, "referendum has to be of type settings");

  uint64_t min_test_cycles = this->m_contract.config_get("refmintest"_n);
  uint64_t test_cycles = std::get<uint64_t>(args["test_cycles"]);
  check(test_cycles >= min_test_cycles, "the number of test cycles must be at least " + std::to_string(min_test_cycles));

  uint64_t min_eval_cycles = this->m_contract.config_get("refmineval"_n);
  uint64_t eval_cycles = std::get<uint64_t>(args["eval_cycles"]);
  check(eval_cycles >= min_eval_cycles, "the number of eval cycles must be at least " + std::to_string(min_eval_cycles));

  proposals_t.modify(ritr, contract_name, [&](auto & item) {
    item.title = std::get<string>(args["title"]);
    item.summary = std::get<string>(args["summary"]);
    item.description = std::get<string>(args["description"]);
    item.image = std::get<string>(args["image"]);
    item.url = std::get<string>(args["url"]);
  });

  propaux_t.modify(raitr, contract_name, [&](auto & item){
    item.special_attributes.at("setting_name") = setting_name;
    item.special_attributes.at("is_float") = s_info->is_float;
    if (s_info->is_float) {
      item.special_attributes.at("new_value") = std::get<double>(args["new_value"]);
      item.special_attributes.at("previous_value") = s_info->previous_value_double;
    } else {
      item.special_attributes.at("new_value") = std::get<uint64_t>(args["new_value"]);
      item.special_attributes.at("previous_value") = s_info->previous_value_uint;
    }
    item.special_attributes.at("cycles_per_status") = "1," + std::to_string(test_cycles) + "," + std::to_string(eval_cycles);
  });

}

void ReferendumSettings::cancel (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  auto ritr = proposals_t.require_find(proposal_id, "referendum not found");
  auto raitr = propaux_t.require_find(proposal_id, "refaux entry not found");

  check(ritr->stage == ProposalsCommon::stage_staged, "can not cancel referendum, it is not staged");
  check(ritr->type == ProposalsCommon::type_ref_setting, "referendum has to be of type settings");

  proposals_t.erase(ritr);
  propaux_t.erase(raitr);

}

void ReferendumSettings::evaluate (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t proposal_id = std::get<uint64_t>(args["proposal_id"]);
  uint64_t propcycle = std::get<uint64_t>(args["propcycle"]);

  dao::proposal_tables proposals_t(contract_name, contract_name.value);
  dao::proposal_auxiliary_tables propaux_t(contract_name, contract_name.value);

  auto ritr = proposals_t.require_find(proposal_id, "referendum not found");
  auto raitr = propaux_t.require_find(proposal_id, "refaux entry not found");

  name setting_name = std::get<name>(raitr->special_attributes.at("setting_name"));
  bool is_float = std::get<bool>(raitr->special_attributes.at("is_float"));

  name current_stage = ritr->stage;
  name current_status = ritr->status;

  if (current_stage == ProposalsCommon::stage_active) {
    
    uint64_t required_unity = get_required_unity(setting_name, is_float);
    bool unity_passed = utils::is_valid_majority(ritr->favour, ritr->against, required_unity);

    bool quorum_passed = true;
    if (current_status == ProposalsCommon::status_voting) {
      dao::size_tables sizes_votes_t(contract_name, ProposalsCommon::vote_scope.value);
      auto total_voters_itr = sizes_votes_t.find(proposal_id);

      // dao::size_tables sizes_t(contracts::voice, contracts::voice.value);
      dao::size_tables sizes_t(contracts::proposals, contracts::proposals.value);
      auto total_citizens_itr = sizes_t.require_find(name("voice.sz").value, "voice size not found");

      uint64_t required_quorum = get_required_quorum(setting_name, is_float);
      quorum_passed = utils::is_valid_quorum(total_voters_itr->size, required_quorum, total_citizens_itr->size);
    }

    if (unity_passed && quorum_passed) {
      if (ritr->age == 0) {
        this->m_contract.send_inline_action(
          permission_level(contract_name, "active"_n),
          contracts::token,
          "transfer"_n,
          std::make_tuple(contract_name, ritr->creator, ritr->staked, string("refund"))
        );
      }

      uint64_t new_age = ritr->age + 1;
      string cycles_per_status = std::get<string>(raitr->special_attributes.at("cycles_per_status"));
      name next_status = get_next_status(cycles_per_status, new_age);
      
      if (next_status == ProposalsCommon::status_evaluate && current_status == ProposalsCommon::status_test) {
        if (is_float) {
          change_setting(setting_name, std::get<double>(raitr->special_attributes.at("new_value")), is_float);
        }
        else {
          change_setting(setting_name, std::get<uint64_t>(raitr->special_attributes.at("new_value")), is_float);
        }
      }

      proposals_t.modify(ritr, contract_name, [&](auto & item){
        item.age = new_age;
        item.last_ran_cycle = propcycle;
        item.status = next_status;
        if (next_status == ProposalsCommon::status_passed) {
          item.stage = ProposalsCommon::stage_done;
        }
      });

    } else {
      if (current_status == ProposalsCommon::status_voting) {
        this->m_contract.send_inline_action(
          permission_level(contract_name, "active"_n),
          contracts::token,
          "burn"_n,
          std::make_tuple(contract_name, ritr->staked)
        );
      }

      if (current_status == ProposalsCommon::status_evaluate) {
        if (is_float) {
          change_setting(setting_name, std::get<double>(raitr->special_attributes.at("previous_value")), is_float);
        }
        else {
          change_setting(setting_name, std::get<uint64_t>(raitr->special_attributes.at("previous_value")), is_float);
        }
      }

      proposals_t.modify(ritr, contract_name, [&](auto & item){
        item.stage = ProposalsCommon::stage_done;
        item.status = ProposalsCommon::status_rejected;
        item.last_ran_cycle = propcycle;
      });
    }

    this->m_contract.size_change(this->m_contract.prop_active_size, -1);

  } else if (ritr->stage == ProposalsCommon::stage_staged) {

    uint64_t price_amount = this->m_contract.config_get("refsnewprice"_n);
    if (ritr->staked.amount < price_amount) { return; }

    proposals_t.modify(ritr, contract_name, [&](auto & item){
      item.stage = ProposalsCommon::stage_active;
      item.status = ProposalsCommon::status_voting;
      item.last_ran_cycle = propcycle;
    });

    this->m_contract.size_change(this->m_contract.prop_active_size, 1);

  }

}

name ReferendumSettings::get_scope () {
  return this->m_contract.referendums_scope;
}

name ReferendumSettings::get_fund_type () {
  return ProposalsCommon::fund_type_none;
}

void ReferendumSettings::check_can_vote (const name & status, const name & stage) {
  check(status == ProposalsCommon::status_voting, "can not vote, proposal is not in voting status");
}

uint64_t ReferendumSettings::get_required_unity (const name & setting, const bool & is_float) {
  name impact;

  dao::config_tables config_t(contracts::settings, contracts::settings.value);

  if (is_float) {
    dao::config_float_tables fconfig_t(contracts::settings, contracts::settings.value);
    auto fitr = fconfig_t.find(setting.value);
    impact = fitr->impact;
  } else {
    auto citr = config_t.find(setting.value);
    impact = citr->impact;
  }

  switch (impact.value) {
    case high_impact.value:
      return config_t.find(name("unity.high").value) -> value;
    case medium_impact.value:
      return config_t.find(name("unity.medium").value) -> value;
    case low_impact.value:
      return config_t.find(name("unity.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      return 0;
  }
}

uint64_t ReferendumSettings::get_required_quorum (const name & setting, const bool & is_float) {
  name impact;

  dao::config_tables config_t(contracts::settings, contracts::settings.value);

  if (is_float) {
    dao::config_float_tables fconfig_t(contracts::settings, contracts::settings.value);
    auto fitr = fconfig_t.find(setting.value);
    impact = fitr->impact;
  } else {
    auto citr = config_t.find(setting.value);
    impact = citr->impact;
  }

  switch (impact.value) {
    case high_impact.value:
      return config_t.find(name("quorum.high").value) -> value;
    case medium_impact.value:
      return config_t.find(name("quorum.med").value) -> value;
    case low_impact.value:
      return config_t.find(name("quorum.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      return 0;
  }
}

name ReferendumSettings::get_next_status (const string & cycles_per_status_string, const uint64_t & age) {

  uint64_t accumulated = 0;

  name valid_statuses[3] = { 
    ProposalsCommon::status_voting, 
    ProposalsCommon::status_test, 
    ProposalsCommon::status_evaluate 
  };

  string val("");
  int j = 0;

  for (int i = 0; i < cycles_per_status_string.size(); i++) {
    if (cycles_per_status_string[i] == ',') {

      accumulated += uint64_t(std::stoi(val));

      if (age < accumulated) {
        return valid_statuses[j];
      }
      j++;
      val = "";
    }
    else {
      val += cycles_per_status_string[i];
    }
  }

  accumulated += uint64_t(std::stoi(val));

  if (age < accumulated) {
    return valid_statuses[j];
  }

  return ProposalsCommon::status_passed;

}

ReferendumSettings::SettingInfo * ReferendumSettings::get_setting_info (const name & setting_name) {

  dao::config_tables config(contracts::settings, contracts::settings.value);

  SettingInfo * s_info = new SettingInfo();
  s_info->is_float = false;

  uint64_t previous_value_uint;
  double previous_value_double;
  bool is_float = false;

  auto citr = config.find(setting_name.value);
  if (citr != config.end()) {
    s_info->previous_value_uint = citr->value;
  } else {
    dao::config_float_tables config_f(contracts::settings, contracts::settings.value);

    auto cfitr = config_f.require_find(setting_name.value, "setting not found");
    s_info->previous_value_double = cfitr->value;
    s_info->is_float = true;
  }

  return s_info;

}

template <typename T>
void ReferendumSettings::change_setting (const name & setting_name, const T & setting_value, const bool & is_float) {

  name action = is_float ? "conffloat"_n : "configure"_n;

  this->m_contract.send_inline_action(
    permission_level(contracts::settings, "referendum"_n),
    contracts::settings,
    action,
    std::make_tuple(setting_name, setting_value)
  );

}

