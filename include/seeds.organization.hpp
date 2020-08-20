#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables.hpp>
#include <tables/config_table.hpp>

using namespace eosio;
using std::string;

CONTRACT organization : public contract {

    public:
        using contract::contract;
        organization(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              organizations(receiver, receiver.value),
              sponsors(receiver, receiver.value),
              apps(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              balances(contracts::harvest, contracts::harvest.value),
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

        ACTION create(name sponsor, name orgaccount, string orgfullname, string publicKey);

        ACTION destroy(name orgname, name sponsor);

        ACTION refund(name beneficiary, asset quantity);

        ACTION registerapp(name owner, name organization, name appname, string applongname);

        ACTION banapp(name appname);

        ACTION appuse(name appname, name account);

        ACTION cleandaus();

        ACTION cleandau(name appname, uint64_t timestamp, uint64_t start);

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
            asset planted;

            uint64_t primary_key() const { return org_name.value; }
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

        TABLE vote_table {
            name account;
            uint64_t timestamp;
            int64_t regen_points;

            uint64_t primary_key() const { return account.value; }
        };

        DEFINE_CONFIG_TABLE
        
        DEFINE_CONFIG_TABLE_MULTI_INDEX

        TABLE app_table {
            name app_name;
            name org_name;
            string app_long_name;
            bool is_banned;
            uint64_t number_of_uses;

            uint64_t primary_key() const { return app_name.value; }
            uint64_t by_org() const { return org_name.value; }
        };

        TABLE dau_table {
            name account;
            uint64_t date;
            uint64_t number_app_uses;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE dau_history_table {
            uint64_t dau_history_id;
            name account;
            uint64_t date;
            uint64_t number_app_uses;

            uint64_t primary_key() const { return dau_history_id; }
            uint64_t by_account() const { return account.value; }
            uint64_t by_date() const { return date; }
        };

        typedef eosio::multi_index<"balances"_n, tables::balance_table,
            indexed_by<"byplanted"_n,
            const_mem_fun<tables::balance_table, uint64_t, &tables::balance_table::by_planted>>
        > balance_tables;
    
        balance_tables balances;

        typedef eosio::multi_index <"organization"_n, organization_table> organization_tables;

        typedef eosio::multi_index <"members"_n, members_table> members_tables;

        typedef eosio::multi_index <"sponsors"_n, sponsors_table> sponsors_tables;

        typedef eosio::multi_index <"votes"_n, vote_table> vote_tables;

        typedef eosio::multi_index<"users"_n, tables::user_table,
            indexed_by<"byreputation"_n,
            const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
        > user_tables;

        typedef eosio::multi_index<"apps"_n, app_table,
            indexed_by<"byorg"_n,
            const_mem_fun<app_table, uint64_t, &app_table::by_org>>
        > app_tables;

        typedef eosio::multi_index<"daus"_n, dau_table> dau_tables;

        typedef eosio::multi_index<"dauhistory"_n, dau_history_table,
            indexed_by<"byaccount"_n,
            const_mem_fun<dau_history_table, uint64_t, &dau_history_table::by_account>>,
            indexed_by<"bydate"_n,
            const_mem_fun<dau_history_table, uint64_t, &dau_history_table::by_date>>
        > dau_history_tables;

        organization_tables organizations;
        sponsors_tables sponsors;
        user_tables users;
        config_tables config;
        app_tables apps;

        const name min_planted = "org.minplant"_n;

        void create_account(name sponsor, name orgaccount, string fullname, string publicKey);
        void check_owner(name organization, name owner);
        void init_balance(name account);
        int64_t getregenp(name account);
        void check_user(name account);
        void vote(name organization, name account, int64_t regen);
        void check_asset(asset quantity);
        uint64_t get_beginning_of_day_in_seconds();
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<organization>(name(receiver), name(code), &organization::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(organization, (reset)(addmember)(removemember)(changerole)(changeowner)(addregen)
            (subregen)(create)(destroy)(refund)(appuse)(registerapp)(banapp)(cleandaus)(cleandau))
      }
  }
}

