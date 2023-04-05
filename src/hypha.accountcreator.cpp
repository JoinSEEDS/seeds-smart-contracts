#include <hypha.accountcreator.hpp>

void joinhypha::setconfig ( const name& account_creator_contract, const name& account_creator_oracle ) {
   require_auth (get_self());

   check ( is_account (account_creator_contract), "Provided account creator contract is not a Telos account: " + account_creator_contract.to_string());
   check ( is_account (account_creator_oracle), "Provided account creator oracle is not a Telos account: " + account_creator_oracle.to_string());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.account_creator_contract = account_creator_contract;
   c.account_creator_oracle = account_creator_oracle;
   config_s.set(c, get_self());
}

void joinhypha::setsetting ( const name& setting_name, const uint8_t& setting_value ) {
   require_auth (get_self());

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());
   c.settings[setting_name] = setting_value;
   config_s.set(c, get_self());
}

void joinhypha::pause () {
   setsetting ("active"_n, 0);
}

void joinhypha::activate () {
   setsetting ("active"_n, 1);
}

void joinhypha::create ( const name& account_to_create, const string& key) {

   config_table      config_s (get_self(), get_self().value);
   config c = config_s.get_or_create (get_self(), config());

   require_auth (c.account_creator_oracle);

   uint8_t paused = c.settings[name("active")];
   check (c.settings["active"_n] == 1, "Contract is not active. Exiting.");

   //string prefix { "EOS" };

   print (" Account Creator Oracle   : ", c.account_creator_oracle.to_string(), "\n");

   // print (" Self                       : ", get_self().to_string(), "\n");
   // print (" Owner Key                  : ", owner_key, "\n");
   // print (" Active Key                 : ", active_key, "\n");
   // print (" Prefix                     : ", prefix, "\n");

    create_account(account_to_create, key);

}

void joinhypha::create_account(name account, string publicKey)
{
  if (is_account(account))
    return;

  authority auth = keystring_authority(publicKey);

  action(
      permission_level{_self, "active"_n},
      "eosio"_n, "newaccount"_n,
      make_tuple(_self, account, auth, auth))
      .send();

  action(
      permission_level{_self, "active"_n},
      "eosio"_n, 
      "buyrambytes"_n,
      make_tuple(_self, account, 2777)) // 2000 RAM is used by Telos free.tf
      .send();

  action(
      permission_level{_self, "active"_n},
      "eosio"_n, "delegatebw"_n,
      make_tuple(_self, account, asset(5000, network_symbol), asset(5000, network_symbol), 0))
      .send();
}