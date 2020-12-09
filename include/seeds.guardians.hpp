#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <tables/user_table.hpp>
#include <abieos_numeric.hpp>
#include <contracts.hpp>
#include <string>

using namespace eosio;
using namespace std;
using namespace abieos;

enum class status : name {
    setup = "setup"_n,
    active = "active"_n,
    paused = "paused"_n,
    recovery = "recovery"_n,
    complete = "complete"_n,
};

CONTRACT guardians : public contract
{
public:
    using contract::contract;
    guardians(name receiver, name code, datastream<const char *> ds)
        : contract(receiver, code, ds),
          protectables(receiver, receiver.value)
    {
    }

    ACTION reset();
    ACTION setup(name protectable_account, uint64_t claim_delay_sec, checksum256 guardian_invite_hash);
    ACTION protect(name guardian_account, name protectable_account, checksum256 guardian_invite_secret);
    ACTION protectby(name protectable_account, name guardian_account);
    ACTION activate(name protectable_account);
    ACTION pause(name protectable_account);
    ACTION unpause(name protectable_account);
    ACTION deleteguards(name protectable_account);
    ACTION newrecovery(name guardian_account, name protectable_account, string new_public_key);
    ACTION yesrecovery(name guardian_account, name protectable_account);
    ACTION norecovery(name guardian_account, name protectable_account);
    ACTION stoprecovery(name protectable_account);
    ACTION claim(name protectable_account);

private:
    int min_guardians = 3;
    int max_guardians = 5;
    int quorum_percent = 60;

    void clear_guardians(name protectable_account);
    void clear_approvals(name protectable_account);
    void require_guardian(name protectable_account, name guardian_account);
    void approve_recovery(name protectable_account, name guardian_account);
    void require_status(name protectable_account, status required_status)
    void add_guardian(name protectable_account, name guardian_account);
    void change_account_permission(name user_account, string public_key);
    bool is_seeds_user(name account);
    authority guardian_key_authority(string key_str);

    TABLE protectable_table
    {
        name account;
        name current_status;
        checksum256 guardian_invite_hash;
        string proposed_public_key;
        uint64_t claim_delay_sec;

        uint64_t primary_key() const { return account.value; }
    }

    TABLE guardians_table
    {
        name account;
        uint64_t primary_key() const { return account.value; }
    };

    TABLE approval_table
    {
        name account;
        uint64_t primary_key() const { return account.value; }
    };

    typedef eosio::multi_index<"protectables"_n, protectable_table> protectable_tables;
    typedef eosio::multi_index<"guards"_n, guardians_table> guardian_tables;
    typedef eosio::multi_index<"approvals"_n, approval_table> approval_tables;

    protectable_tables protectables;
};

EOSIO_DISPATCH(guardians, (reset)(setup)(protect)(protectby)(activate)(pause)(unpause)(deleteguards)(newrecovery)(yesrecovery)(norecovery)(stoprecovery)(claim));