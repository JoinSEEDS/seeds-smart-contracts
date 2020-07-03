#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>

using namespace eosio;
using std::string;

CONTRACT bioregion : public contract {
    public:
        using contract::contract;
        bioregion(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              bioregions(receiver, receiver.value),
              sponsors(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value)
              {}
        
        
        // ACTION addorg(name org, name owner);

        ACTION reset();

        // ACTION addmember(name organization, name owner, name account, name role); // (manager / owner permission)
        
        // ACTION removemember(name organization, name owner, name account); // (manager owner permission)
        
        // ACTION changerole(name organization, name owner, name account, name new_role); // (manager / owner permission)
        
        // ACTION changeowner(name organization, name owner, name account); // (owner permission)

        // ACTION addregen(name organization, name account); // adds a fixed number of regen points, possibly modified by the account reputation. account - account adding the regen (account auth)
        
        // ACTION subregen(name organization, name account); // same as add, just negative (account auth)

        // ACTION create(name sponsor, name orgaccount, string orgfullname, string publicKey);

        // ACTION destroy(name orgname, name sponsor);

        // ACTION refund(name beneficiary, asset quantity);

        void deposit(name from, name to, asset quantity, std::string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);

        void check_owner(name organization, name owner);
        void init_balance(name account);
        void check_user(name account);

        TABLE bioregion_table {
            name id;
            name founder;
            name status; // "active" "inactive"
            string description;
            string locationjson; // json description of the area
            float latitude;
            float longitude;

            uint64_t primary_key() const { return id.value; }
        };

        TABLE members_table {
            name account;
            name role;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE sponsors_table { // is it posible to have a negative balance?
            name account;
            asset balance;

            uint64_t primary_key() const { return account.value; } 
        };
        
        // External tables

        DEFINE_USER_TABLE

        DEFINE_USER_TABLE_MULTI_INDEX

        user_tables users;
        
        DEFINE_CONFIG_TABLE

        DEFINE_CONFIG_TABLE_MULTI_INDEX

        config_tables config;

    
        typedef eosio::multi_index <"bioregions"_n, bioregion_table> bioregion_tables;

        typedef eosio::multi_index <"members"_n, members_table> members_tables;

        typedef eosio::multi_index <"sponsors"_n, sponsors_table> sponsors_tables;

        bioregion_tables bioregions;
        sponsors_tables sponsors;
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<bioregion>(name(receiver), name(code), &bioregion::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(bioregion, (reset))
      }
  }
}

