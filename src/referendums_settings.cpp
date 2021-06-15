#include <proposals/referendum_settings.hpp>

void ReferendumSettings::create (std::map<std::string, VariantValue> & args) {

  referendums::config_tables config(contracts::settings, contracts::settings.value);

  uint64_t previous_value_uint;
  double previous_value_double;
  bool is_float = false;

  name setting_name = std::get<name>(args["setting_name"]);

  auto citr = config.find(setting_name.value);
  if (citr != config.end()) {
    previous_value_uint = citr->value;
  } else {
    referendums::config_float_tables config_f(contracts::settings, contracts::settings.value);

    auto cfitr = config_f.require_find(setting_name.value, "setting not found");
    previous_value_double = cfitr->value;
    is_float = true;
  }

  referendums::referendum_tables referendums_t(this->m_contract.get_self(), this->m_contract.get_self().value);

  referendums_t.emplace(this->m_contract.get_self(), [&](auto & item) {
    item.referendum_id = referendums_t.available_primary_key();
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
    item.special_attributes.insert(std::make_pair("setting_name", setting_name));
    item.special_attributes.insert(std::make_pair("is_float", is_float));
    if (is_float) {
      item.special_attributes.insert(std::make_pair("setting_value", std::get<double>(args["setting_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", previous_value_double));
    } else {
      item.special_attributes.insert(std::make_pair("setting_value", std::get<uint64_t>(args["setting_value"])));
      item.special_attributes.insert(std::make_pair("previous_value", previous_value_uint));
    }
  });

}

void ReferendumSettings::update (std::map<std::string, VariantValue> & args) {

  print("update implementation");

}

void ReferendumSettings::cancel (std::map<std::string, VariantValue> & args) {

  print("cancel implementation");

}

void ReferendumSettings::evaluate (std::map<std::string, VariantValue> & args) {

  print("evaluate implementation");

}

