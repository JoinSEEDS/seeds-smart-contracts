// https://gitlab.com/telos-kitchen/telos-account-creator-contract/blob/master/include/acctcreator.hpp

#include <eosio/eosio.hpp>

#include <eosio/singleton.hpp>
#include <eosio/multi_index.hpp>

using std::string;
using namespace eosio;

CONTRACT acctcreator : public contract {
   public:
      using contract::contract;

      struct [[eosio::table ]] config {
         name                       account_creator_contract   ;
         name                       account_creator_oracle     ;
         std::map<name, uint8_t>    settings                   ;
      };

      typedef singleton<"config"_n, config> config_table;
      typedef multi_index<"config"_n, config> config_table_placeholder;

      ACTION setconfig ( const name& account_creator_contract, const name& account_creator_oracle );
      ACTION setsetting ( const name& setting_name, const uint8_t& setting_value );
      ACTION pause ();
      ACTION activate ();
      
      ACTION create ( const name& account_to_create, const string& owner_key, const string& active_key);
};
