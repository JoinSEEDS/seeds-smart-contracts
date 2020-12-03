#include <eosio/eosio.hpp>
#include <tables/user_table.hpp>
#include <abieos_numeric.hpp>
#include <contracts.hpp>
#include <string>

using namespace eosio;
using namespace std;
using namespace abieos;

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

    ACTION init(name user_account, vector<name> guardian_accounts);

    ACTION cancel(name user_account);

    ACTION recover(name guardian_account, name user_account, string new_public_key);

private:
    void change_account_permission(name user_account, string public_key);
    bool is_seeds_user(name account);

    TABLE guardians_table
    {
        name account;
        vector<name> guardians;

        uint64_t primary_key() const { return account.value; }
    };

    TABLE recovery_table
    {
        name account;
        vector<name> guardians;
        string public_key;

        uint64_t primary_key() const { return account.value; }
    };

    typedef eosio::multi_index<"guards"_n, guardians_table> guardians_tables;
    typedef eosio::multi_index<"recovers"_n, recovery_table> recovery_tables;

    guardians_tables guards;
    recovery_tables recovers;
};

EOSIO_DISPATCH(guardians, (reset)(init)(cancel)(recover));