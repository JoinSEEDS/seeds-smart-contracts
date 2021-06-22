#include <proposals/referendum_settings.hpp>

void ReferendumSettings::create (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  name setting_name = std::get<name>(args["setting_name"]);

  std::unique_ptr<SettingInfo> s_info = std::unique_ptr<SettingInfo>(get_setting_info(setting_name));

  referendums::referendum_tables referendums_t(contract_name, contract_name.value);
  referendums::referendum_auxiliary_tables refaux_t(contract_name, contract_name.value);

  uint64_t referendum_id = referendums_t.available_primary_key();

  uint64_t min_test_cycles = this->m_contract.config_get("refmintest"_n);
  uint64_t test_cycles = std::get<uint64_t>(args["test_cycles"]);
  check(test_cycles >= min_test_cycles, "the number of test cycles must be at least " + std::to_string(min_test_cycles));

  referendums_t.emplace(contract_name, [&](auto & item) {
    item.referendum_id = referendum_id;
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
    item.status = ReferendumsCommon::status_open;
    item.stage = ReferendumsCommon::stage_staged;
    item.type = ReferendumsCommon::type_settings;
    item.last_ran_cycle = 0;
    item.age = 0;
    item.cycles_per_status = {
      uint64_t(1), // voting
      test_cycles, // test
      this->m_contract.config_get("refmineval"_n) // evaluate
    };
  });

  refaux_t.emplace(contract_name, [&](auto & item){
    item.referendum_id = referendum_id;
    item.special_attributes.insert(std::make_pair("setting_name", setting_name));
    item.special_attributes.insert(std::make_pair("is_float", s_info->is_float));
    if (s_info->is_float) {
      item.special_attributes.insert(std::make_pair("setting_value", std::get<double>(args["setting_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", s_info->previous_value_double));
    } else {
      item.special_attributes.insert(std::make_pair("setting_value", std::get<uint64_t>(args["setting_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", s_info->previous_value_uint));
    }
  });

}

void ReferendumSettings::update (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  name setting_name = std::get<name>(args["setting_name"]);
  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  std::unique_ptr<SettingInfo> s_info = std::unique_ptr<SettingInfo>(get_setting_info(setting_name));

  referendums::referendum_tables referendums_t(contract_name, contract_name.value);
  referendums::referendum_auxiliary_tables refaux_t(contract_name, contract_name.value);

  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");
  auto raitr = refaux_t.require_find(referendum_id, "refaux entry not found");

  check(ritr->stage == ReferendumsCommon::stage_staged, "can not update referendum, it is not staged");
  check(ritr->type == ReferendumsCommon::type_settings, "referendum has to be of type settings");

  uint64_t min_test_cycles = this->m_contract.config_get("refmintest"_n);
  uint64_t test_cycles = std::get<uint64_t>(args["test_cycles"]);
  check(test_cycles >= min_test_cycles, "the number of test cycles must be at least " + std::to_string(min_test_cycles));


  referendums_t.modify(ritr, contract_name, [&](auto & item) {
    item.title = std::get<string>(args["title"]);
    item.summary = std::get<string>(args["summary"]);
    item.description = std::get<string>(args["description"]);
    item.image = std::get<string>(args["image"]);
    item.url = std::get<string>(args["url"]);
    item.cycles_per_status = {
      uint64_t(1), // voting
      test_cycles, // test
      this->m_contract.config_get("refmineval"_n) // evaluate
    };
  });

  refaux_t.modify(raitr, contract_name, [&](auto & item){
    item.special_attributes.at("setting_name") = setting_name;
    item.special_attributes.at("is_float") = s_info->is_float;
    if (s_info->is_float) {
      item.special_attributes.at("setting_value") = std::get<double>(args["setting_value"]);
      item.special_attributes.at("previous_value") = s_info->previous_value_double;
    } else {
      item.special_attributes.at("setting_value") = std::get<uint64_t>(args["setting_value"]);
      item.special_attributes.at("previous_value") = s_info->previous_value_uint;
    }
  });

}

void ReferendumSettings::cancel (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  referendums::referendum_tables referendums_t(contract_name, contract_name.value);
  referendums::referendum_auxiliary_tables refaux_t(contract_name, contract_name.value);

  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");
  auto raitr = refaux_t.require_find(referendum_id, "refaux entry not found");

  check(ritr->stage == ReferendumsCommon::stage_staged, "can not cancel referendum, it is not staged");
  check(ritr->type == ReferendumsCommon::type_settings, "referendum has to be of type settings");

  referendums_t.erase(ritr);
  refaux_t.erase(raitr);

}

void ReferendumSettings::evaluate (std::map<std::string, VariantValue> & args) {

  name contract_name = this->m_contract.get_self();
  uint64_t referendum_id = std::get<uint64_t>(args["referendum_id"]);

  referendums::cycle_tables cycle_t(contract_name, contract_name.value);
  referendums::cycle_table c_t = cycle_t.get();

  referendums::referendum_tables referendums_t(contract_name, contract_name.value);
  referendums::referendum_auxiliary_tables refaux_t(contract_name, contract_name.value);

  auto ritr = referendums_t.require_find(referendum_id, "referendum not found");
  auto raitr = refaux_t.require_find(referendum_id, "refaux entry not found");

  name setting_name = std::get<name>(raitr->special_attributes.at("setting_name"));
  bool is_float = std::get<bool>(raitr->special_attributes.at("is_float"));

  name current_stage = ritr->stage;
  name current_status = ritr->status;

  if (current_stage == ReferendumsCommon::stage_active) {
    
    uint64_t required_unity = get_required_unity(setting_name, is_float);
    bool unity_passed = utils::is_valid_majority(ritr->favour, ritr->against, required_unity);

    bool quorum_passed = true;
    if (current_status == ReferendumsCommon::status_voting) {
      referendums::size_tables sizes_votes_t(contract_name, ReferendumsCommon::vote_scope.value);
      auto total_voters_itr = sizes_votes_t.find(referendum_id);

      referendums::size_tables sizes_t(contracts::voice, contracts::voice.value);
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
          std::make_tuple(contract_name, ritr->creator, ritr->staked, "")
        );
      }

      uint64_t new_age = ritr->age + 1;
      name next_status = get_next_status(ritr->cycles_per_status, new_age);
      
      if (next_status == ReferendumsCommon::status_evaluate && current_status == ReferendumsCommon::status_test) {
        if (is_float) {
          print("the setting is float!\n");
        } else {
          print("the setting is integer!\n");
        }
      }

      referendums_t.modify(ritr, contract_name, [&](auto & item){
        item.age = new_age;
        item.last_ran_cycle = c_t.propcycle;
        item.status = next_status;
        if (next_status == ReferendumsCommon::status_passed) {
          item.stage = ReferendumsCommon::stage_done;
        }
      });

    } else {
      if (current_status == ReferendumsCommon::status_voting) {
        this->m_contract.send_inline_action(
          permission_level(contract_name, "active"_n),
          contracts::token,
          "burn"_n,
          std::make_tuple(contract_name, ritr->staked)
        );
      }

      referendums_t.modify(ritr, contract_name, [&](auto & item){
        item.stage = ReferendumsCommon::stage_done;
        item.status = ReferendumsCommon::status_rejected;
        item.age += 1;
        item.last_ran_cycle = c_t.propcycle;
      });
    }

  } else if (ritr->stage == ReferendumsCommon::stage_staged) {

    uint64_t price_amount = this->m_contract.config_get("refsnewprice"_n);
    if (ritr->staked.amount < price_amount) { return; }

    referendums_t.modify(ritr, contract_name, [&](auto & item){
      item.stage = ReferendumsCommon::stage_active;
      item.status = ReferendumsCommon::status_voting;
      item.last_ran_cycle = c_t.propcycle;
    });

  }

}


uint64_t ReferendumSettings::get_required_unity (const name & setting, const bool & is_float) {
  name contract_name = this->m_contract.get_self();
  name impact;

  referendums::config_tables config_t(contract_name, contract_name.value);

  if (is_float) {
    referendums::config_float_tables fconfig_t(contract_name, contract_name.value);
    auto fitr = fconfig_t.find(setting.value);
    impact = fitr->impact;
  } else {
    auto citr = config_t.find(setting.value);
    impact = citr->impact;
  }

  switch (impact) {
    case high_impact:
      return config_t.find(name("unity.high").value) -> value;
    case medium_impact:
      return config_t.find(name("unity.medium").value) -> value;
    case low_impact:
      return config_t.find(name("unity.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      break;
  }
}

uint64_t ReferendumSettings::get_required_quorum (const name & setting, const bool & is_float) {
  name contract_name = this->m_contract.get_self();
  name impact;

  referendums::config_tables config_t(contract_name, contract_name.value);

  if (is_float) {
    referendums::config_float_tables fconfig_t(contract_name, contract_name.value);
    auto fitr = fconfig_t.find(setting.value);
    impact = fitr->impact;
  } else {
    auto citr = config_t.find(setting.value);
    impact = citr->impact;
  }

  switch (impact) {
    case high_impact:
      return config_t.find(name("quorum.high").value) -> value;
    case medium_impact:
      return config_t.find(name("quorum.med").value) -> value;
    case low_impact:
      return config_t.find(name("quorum.low").value) -> value;
    default:
      check(false, "unknown impact for setting " + setting.to_string());
      break;
  }
}

name ReferendumSettings::get_next_status (const std::vector<uint64_t> & cycles_per_status, const uint64_t & age) {

  if (age < cycles_per_status[0]) {
    return ReferendumsCommon::status_voting;
  } 
  else if (age < cycles_per_status[1]) {
    return ReferendumsCommon::status_test;
  }
  else if (age < cycles_per_status[2]) {
    return ReferendumsCommon::status_evaluate;
  }
  
  return ReferendumsCommon::status_passed;

}

ReferendumSettings::SettingInfo * ReferendumSettings::get_setting_info (const name & setting_name) {

  referendums::config_tables config(contracts::settings, contracts::settings.value);

  SettingInfo * s_info = new SettingInfo();
  s_info->is_float = false;

  uint64_t previous_value_uint;
  double previous_value_double;
  bool is_float = false;

  auto citr = config.find(setting_name.value);
  if (citr != config.end()) {
    s_info->previous_value_uint = citr->value;
  } else {
    referendums::config_float_tables config_f(contracts::settings, contracts::settings.value);

    auto cfitr = config_f.require_find(setting_name.value, "setting not found");
    s_info->previous_value_double = cfitr->value;
    s_info->is_float = true;
  }

  return s_info;

}

