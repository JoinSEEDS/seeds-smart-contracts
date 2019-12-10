#include <eosio/eosio.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT organization : public contract {

    public:
        using contract::contract;
        organization(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              organizations(receiver, receiver.value)
              //users(contracts::accounts, contracts::accounts.value)
            {}
        
        
        // ACTION addorg(name org, name owner);

        ACTION reset();

        ACTION addmember(name organization, name owner, name account, name role); // (manager / owner permission)
        
        ACTION removemember(name organization, name owner, name account); // (manager owner permission)
        
        ACTION changerole(name organization, name owner, name account, name new_role); // (manager / owner permission)
        
        ACTION changeowner(name organization, name owner, name account); // (owner permission)

        ACTION addregen(name organization, name account); // adds a fixed number of regen points, possibly modified by the account reputation. account - account adding the regen (account auth)
        
        ACTION subregen(name organization, name account); // same as add, just negative (account auth)

    private:
        TABLE organization_table {
            name org_name;
            name owner;
            uint64_t status;
            uint64_t regen;
            uint64_t reputation;
            uint64_t voice;

            uint64_t primary_key() const { return org_name.value; }
        };

        TABLE members_table {
            name account;
            name role;

            uint64_t primary_key() const { return account.value; }
        };

        /*
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
        */

        typedef eosio::multi_index <"organization"_n, organization_table> organization_tables;

        typedef eosio::multi_index <"members"_n, members_table> members_tables;

        /*
        typedef eosio::multi_index<"users"_n, user_table,
            indexed_by<"byreputation"_n,
            const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>
        > user_tables;
        */

        organization_tables organizations;
        //user_tables users;

        void check_owner(name organization, name owner);
        uint64_t getregenp(name account);
};


