// https://gitlab.com/telos-kitchen/telos-account-creator-contract/blob/master/src/acctcreator.cpp

#include <acctcreator.hpp>

void acctcreator::setconfig ( const name& account_creator_contract, const name& account_creator_oracle ) {
   require_auth (get_self());

   check ( is_account (account_creator_contract), "Provided account creator contract is not a Telos account: " + account_creator_contract.to_string());
   check ( is_account (account_creator_oracle), "Provided account creator oracle is not a Telos account: " + account_creator_oracle.to_string());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.account_creator_contract = account_creator_contract;
   c.account_creator_oracle = account_creator_oracle;
   config_s.set(c, get_self());
}

void acctcreator::setsetting ( const name& setting_name, const uint8_t& setting_value ) {
   require_auth (get_self());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.settings[setting_name] = setting_value;
   config_s.set(c, get_self());
}

void acctcreator::pause () {
   setsetting ("active"_n, 0);
}

void acctcreator::activate () {
   setsetting ("active"_n, 1);
}

void acctcreator::create ( const name& account_to_create, const string& owner_key, const string& active_key) {

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());

//   require_auth (c.account_creator_oracle);

   uint8_t paused = c.settings[name("active")];
   check (c.settings["active"_n] == 1, "Contract is not active. Exiting.");

   string prefix { "EOS" };

   action (
      permission_level{get_self(), "active"_n},
      c.account_creator_contract, "create"_n,
      std::make_tuple(get_self(), account_to_create, owner_key, active_key, prefix))
   .send();
}
