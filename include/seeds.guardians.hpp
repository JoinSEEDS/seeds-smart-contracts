#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <tables/user_table.hpp>
#include <abieos_numeric.hpp>
#include <contracts.hpp>
#include <string>

using namespace eosio;
using namespace std;
using namespace abieos;

// * scoping by account
// status field
// auto account creation for guardians
// invites by link
// restore by link

CONTRACT guardians : public contract
{
public:
    using contract::contract;
    guardians(name receiver, name code, datastream<const char *> ds)
        : contract(receiver, code, ds),
          guards(receiver, receiver.value),
          recovers(receiver, receiver.value)
    {
    }

    ACTION reset();

    ACTION init(name protectable_account, checksum256 invite_hash);

    ACTION join(name guardian_account, checksum256 invite_secret);

    ACTION activate(name protectable_account);

    ACTION cancel(name protectable_account);

    ACTION recover(name guardian_account, name protectable_account, string new_public_key);

    ACTION claim(name protectable_account);

    ACTION init(name user_account, vector<name> guardian_accounts, uint64_t time_delay_sec);

    ACTION cancel(name user_account);

    ACTION recover(name guardian_account, name user_account, string new_public_key);
    
    ACTION claim(name user_account);

private:
    int min_guardians = 3;
    int max_guardians = 5;
    int quorum_percent = 60;

    void change_account_permission(name user_account, string public_key);
    bool is_seeds_user(name account);
    authority guardian_key_authority(string key_str);

    TABLE guardians_table
    {
        name account;
        vector<name> guardians;
        uint64_t time_delay_sec;

        uint64_t primary_key() const { return account.value; }
    };

    TABLE recovery_table
    {
        name account;
        vector<name> guardians;
        string public_key;
        uint64_t complete_timestamp;

        uint64_t primary_key() const { return account.value; }
    };

    typedef eosio::multi_index<"guards"_n, guardians_table> guardians_tables;
    typedef eosio::multi_index<"recovers"_n, recovery_table> recovery_tables;

    guardians_tables guards;
    recovery_tables recovers;
};

EOSIO_DISPATCH(guardians, (reset)(init)(cancel)(recover)(claim));