#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT organization : public contract {

    public:
        using contract::contract;
        organization(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              organizations(receiver, receiver.value),
              balances(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value)
              {}
        
        
        // ACTION addorg(name org, name owner);

        ACTION reset();

        ACTION addmember(name organization, name owner, name account, name role); // (manager / owner permission)
        
        ACTION removemember(name organization, name owner, name account); // (manager owner permission)
        
        ACTION changerole(name organization, name owner, name account, name new_role); // (manager / owner permission)
        
        ACTION changeowner(name organization, name owner, name account); // (owner permission)

        ACTION addregen(name organization, name account); // adds a fixed number of regen points, possibly modified by the account reputation. account - account adding the regen (account auth)
        
        ACTION subregen(name organization, name account); // same as add, just negative (account auth)

        ACTION create(name orgname, name sponsor);

        ACTION destroy(name orgname, name sponsor);

        ACTION refund(name beneficiary, asset quantity);

        void deposit(name from, name to, asset quantity, std::string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);

        TABLE organization_table {
            name org_name;
            name owner;
            uint64_t status;
            int64_t regen;
            uint64_t reputation;
            uint64_t voice;
            asset fee;

            uint64_t primary_key() const { return org_name.value; }
        };

        TABLE members_table {
            name account;
            name role;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE balances_table { // is it posible to have a negative balance?
            name account;
            asset balance;

            uint64_t primary_key() const { return account.value; } 
        };

        TABLE vote_table {
            name account;
            uint64_t timestamp; // is it relevant?
            int64_t regen_points;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE user_table {
            name account;
            name status;
            name type;
            string nickname;
            string image;
            string story;
            string roles;
            string skills;
            string interests;
            uint64_t reputation;
            uint64_t timestamp;

            uint64_t primary_key()const { return account.value; }
            uint64_t by_reputation()const { return reputation; }
        };

        TABLE config_table {
            name param;
            uint64_t value;
            uint64_t primary_key()const { return param.value; }
        };


        typedef eosio::multi_index <"organization"_n, organization_table> organization_tables;

        typedef eosio::multi_index <"members"_n, members_table> members_tables;

        typedef eosio::multi_index <"balances"_n, balances_table> balances_tables;

        typedef eosio::multi_index <"votes"_n, vote_table> vote_tables;

        typedef eosio::multi_index<"users"_n, user_table,
            indexed_by<"byreputation"_n,
            const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>
        > user_tables;

        typedef eosio::multi_index<"config"_n, config_table> config_tables;


        organization_tables organizations;
        balances_tables balances;
        user_tables users;
        config_tables config;

        const name fee = "fee"_n;

        void check_owner(name organization, name owner);
        void init_balance(name account);
        int64_t getregenp(name account);
        void check_user(name account);
        void vote(name organization, name account, int64_t regen);
        void check_asset(asset quantity);
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<organization>(name(receiver), name(code), &organization::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(organization, (reset)(addmember)(removemember)(changerole)(changeowner)(addregen)(subregen)(create)(destroy)(refund))
      }
  }
}

