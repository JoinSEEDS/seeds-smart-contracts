#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
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
              members(receiver, receiver.value),
              sponsors(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value)
              {}
        
        
        ACTION create(
            name founder, 
            name bioaccount, 
            string description, 
            string locationJson, 
            float latitude, 
            float longitude, 
            string publicKey);

        ACTION join(name bioregion, name account);
        ACTION leave(name bioregion, name account);

        ACTION addrole(name bioregion, name admin, name account, name role);
        ACTION removerole(name bioregion, name admin, name account);
        ACTION leaverole(name bioregion, name account);
        ACTION removemember(name bioregion, name admin, name account);

        ACTION setfounder(name bioregion, name founder, name new_founder);

        ACTION reset();

        void deposit(name from, name to, asset quantity, std::string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);
        
        name founder_role = name("founder");
        name admin_role = name("admin");
        

        void auth_founder(name bioregion, name founder);
        void init_balance(name account);
        void check_user(name account);
        void remove_member(name account);
        void create_telos_account(name sponsor, name orgaccount, string publicKey); 
        void size_change(name bioregion, int delta);
        void delete_role(name bioregion, name account);
        bool is_member(name bioregion, name account);
        bool is_admin(name bioregion, name account);

        TABLE bioregion_table {
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
        };

        typedef eosio::multi_index <"bioregions"_n, bioregion_table,
            indexed_by<"bystatus"_n,const_mem_fun<bioregion_table, uint64_t, &bioregion_table::by_status>>,
            indexed_by<"bycount"_n,const_mem_fun<bioregion_table, uint64_t, &bioregion_table::by_count>>
        > bioregion_tables;


        TABLE members_table {
            name bioregion;
            name account;
            time_point joined_date = current_block_time().to_time_point();

            uint64_t primary_key() const { return account.value; }
            uint64_t by_bio() const { return bioregion.value; }

        };
        typedef eosio::multi_index <"members"_n, members_table,
            indexed_by<"bybio"_n,const_mem_fun<members_table, uint64_t, &members_table::by_bio>>
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


        TABLE sponsors_table { // is it posible to have a negative balance?
            name account;
            asset balance;

            uint64_t primary_key() const { return account.value; } 
        };
        typedef eosio::multi_index <"sponsors"_n, sponsors_table> sponsors_tables;

        // External tables

        DEFINE_USER_TABLE

        DEFINE_USER_TABLE_MULTI_INDEX

        user_tables users;
        
        DEFINE_CONFIG_TABLE

        DEFINE_CONFIG_TABLE_MULTI_INDEX

        config_tables config;

        bioregion_tables bioregions;
        members_tables members;
        sponsors_tables sponsors;
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<bioregion>(name(receiver), name(code), &bioregion::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(bioregion, (reset)(create)(join)(leave)(addrole)(removerole)(removemember)(leaverole)(setfounder))
      }
  }
}

