#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/config_float_table.hpp>
#include <tables/size_table.hpp>

using namespace eosio;
using std::string;

CONTRACT region : public contract {
    public:
        using contract::contract;
        region(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              regions(receiver, receiver.value),
              members(receiver, receiver.value),
              sponsors(receiver, receiver.value),
              regiondelays(receiver, receiver.value),
              sizes(receiver, receiver.value),
              harvestbalances(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value),
              configfloat(contracts::settings, contracts::settings.value)
              {}
        
        
        ACTION create(
            name founder, 
            name rgnaccount, 
            string description, 
            string locationJson, 
            float latitude, 
            float longitude, 
            string publicKey);

        ACTION join(name region, name account);
        ACTION leave(name region, name account);

        ACTION addrole(name region, name admin, name account, name role);
        ACTION removerole(name region, name admin, name account);
        ACTION leaverole(name region, name account);
        ACTION removemember(name region, name admin, name account);

        ACTION setfounder(name region, name founder, name new_founder);

        ACTION reset();

        ACTION removerdc(name region);


        void deposit(name from, name to, asset quantity, std::string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);
        symbol test_symbol = symbol("TESTS", 4);
        
        name founder_role = name("founder");
        name admin_role = name("admin");
        
        const name status_inactive = name("inactive");
        const name status_active = name("active");

        const name active_size = name("active.sz");

        void auth_founder(name region, name founder);
        void init_balance(name account);
        void check_user(name account);
        void remove_member(name account);
        void create_telos_account(name sponsor, name orgaccount, string publicKey); 
        void size_change(name id, int delta);
        void delete_role(name region, name account);
        bool is_member(name region, name account);
        bool is_admin(name region, name account);
        double config_float_get(name key);
        uint64_t config_get(name key);
        void update_members_count(name region, int delta);
        void add_harvest_balance(name region, asset amount);

        TABLE region_table {
            name id;
            name founder;
            name status; // "active" "inactive"
            string description;
            string locationjson; // json description of the area
            float latitude;
            float longitude;
            uint64_t members_count;
            time_point created_at = current_block_time().to_time_point();

            uint64_t primary_key() const { return id.value; }
            uint64_t by_status() const { return status.value; }
            uint64_t by_count() const { return members_count; }
            uint128_t by_status_id() const { return (uint128_t(status.value) << 64) + id.value; }
        };

        typedef eosio::multi_index <"regions"_n, region_table,
            indexed_by<"bystatus"_n,const_mem_fun<region_table, uint64_t, &region_table::by_status>>,
            indexed_by<"bycount"_n,const_mem_fun<region_table, uint64_t, &region_table::by_count>>,
            indexed_by<"bystatusid"_n,const_mem_fun<region_table, uint128_t, &region_table::by_status_id>>
        > region_tables;


        TABLE members_table {
            name region;
            name account;
            time_point joined_date = current_block_time().to_time_point();

            uint64_t primary_key() const { return account.value; }
            uint64_t by_region() const { return region.value; }

        };
        typedef eosio::multi_index <"members"_n, members_table,
            indexed_by<"byregion"_n,const_mem_fun<members_table, uint64_t, &members_table::by_region>>
        > members_tables;


        TABLE roles_table {
            name account;
            name role;
            time_point joined_date = current_block_time().to_time_point();

            uint64_t primary_key() const { return account.value; }
            uint64_t by_role() const { return role.value; }

        };
        typedef eosio::multi_index <"roles"_n, roles_table,
            indexed_by<"byrole"_n,const_mem_fun<roles_table, uint64_t, &roles_table::by_role>>
        > roles_tables;


        TABLE sponsors_table {
            name account;
            asset balance;

            uint64_t primary_key() const { return account.value; } 
        };
        typedef eosio::multi_index <"sponsors"_n, sponsors_table> sponsors_tables;

        TABLE delay_table {
            name account;
            bool apply_vote_delay;
            uint64_t joined_date_timestamp;

            uint64_t primary_key() const { return account.value; }
        };
        typedef eosio::multi_index <"regiondelays"_n, delay_table> delay_tables;

        TABLE harvest_balance_table {
            name region;
            asset balance;

            uint64_t primary_key() const { return region.value; }
        };
        typedef eosio::multi_index <"hrvstrdcblnc"_n, harvest_balance_table> harvest_balance_tables;

        // External tables

        DEFINE_USER_TABLE

        DEFINE_USER_TABLE_MULTI_INDEX

        user_tables users;
        
        DEFINE_CONFIG_TABLE

        DEFINE_CONFIG_TABLE_MULTI_INDEX

        DEFINE_CONFIG_FLOAT_TABLE

        DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

        DEFINE_SIZE_TABLE

        DEFINE_SIZE_TABLE_MULTI_INDEX

        config_tables config;
        config_float_tables configfloat;
        size_tables sizes;

        region_tables regions;
        members_tables members;
        sponsors_tables sponsors;
        delay_tables regiondelays;
        harvest_balance_tables harvestbalances;
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<region>(name(receiver), name(code), &region::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(region, (reset)(create)(join)(leave)(addrole)(removerole)
          (removemember)(leaverole)(setfounder)(removerdc))
      }
  }
}

