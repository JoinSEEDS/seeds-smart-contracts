#include <seeds.guardians.hpp>
#include <set>

void guardians::reset()
{
    require_auth(get_self());

    auto gitr = guards.begin();
    while (gitr != guards.end())
    {
        gitr = guards.erase(gitr);
    }

    auto ritr = recovers.begin();
    while (ritr != recovers.end())
    {
        ritr = recovers.erase(ritr);
    }
}

void guardians::init(name user_account, vector<name> guardian_accounts, uint64_t time_delay_sec)
{
    require_auth(user_account);

    auto gitr = guards.find(user_account.value);

    check(gitr == guards.end(), "account " + user_account.to_string() + " already has guardians");

    check(guardian_accounts.size() >= 3,
          "provided " + to_string(guardian_accounts.size()) + " guardians, but needed at least 3 guardians");
    
    vector<name> guardians_unique;

    for (std::size_t i = 0; i < guardian_accounts.size(); i++)
    {
        name guard = guardian_accounts[i];
        
        check(user_account != guard, "user cannot be their own guardiam");
        
        check(is_seeds_user(guard), "guardian " + guard.to_string() + " is not a seeds user");
        
        for (std::size_t k = 0; k < guardians_unique.size(); k++) {
            if (guardians_unique[k] == guard) {
                check(false, "duplicate guardian in list "+guard.to_string());
            }
        }
        guardians_unique.push_back(guard);
    }

    guards.emplace(get_self(), [&](auto &item) {
        item.account = user_account;
        item.guardians = guardian_accounts;
        item.time_delay_sec = time_delay_sec;
    });
}

void guardians::cancel(name user_account)
{
    require_auth(user_account);

    auto gitr = guards.find(user_account.value);

    check(gitr != guards.end(),
          "account " + user_account.to_string() + " does not have guards");

    guards.erase(gitr);

    auto ritr = recovers.find(user_account.value);

    if (ritr != recovers.end())
    {
        recovers.erase(ritr);
    }
}

void guardians::recover(name guardian_account, name user_account, string new_public_key)
{
    require_auth(guardian_account);

    auto gitr = guards.find(user_account.value);

    check(gitr != guards.end(),
          "account " + user_account.to_string() + " does not have guardians");

    bool is_user_guardian = false;

    for (std::size_t i = 0; i < gitr->guardians.size(); i++)
    {
        if (gitr->guardians[i] == guardian_account)
        {
            is_user_guardian = true;
        }
    }

    check(is_user_guardian == true,
          "account " + guardian_account.to_string() +
              " is not a guardian for " + user_account.to_string());

    auto ritr = recovers.find(user_account.value);

    if (ritr == recovers.end())
    {
        recovers.emplace(get_self(), [&](auto &item) {
            item.account = user_account;
            item.guardians = vector{guardian_account};
            item.public_key = new_public_key;
            item.complete_timestamp = 0;
        });
    }
    else
    {
        if (ritr->public_key.compare(new_public_key) == 0)
        {
            bool is_guardian_recovering = false;

            for (std::size_t i = 0; i < ritr->guardians.size(); i++)
            {
                if (ritr->guardians[i] == guardian_account)
                {
                    is_guardian_recovering = true;
                }
            }

            check(is_guardian_recovering == false,
                  "guardian " + guardian_account.to_string() + " already recovering " + user_account.to_string());

            recovers.modify(ritr, get_self(), [&](auto &item) {
                item.guardians.push_back(guardian_account);
            });
        }
        else
        {
            check(ritr -> complete_timestamp == 0, "recovery complete, waiting for claim - can't change key now");

            recovers.modify(ritr, get_self(), [&](auto &item) {
                item.guardians = vector{guardian_account};
                item.public_key = new_public_key;
                item.complete_timestamp = 0;
            });
        }
    }

    int required_guardians = gitr->guardians.size();
    int signed_guardians = ritr->guardians.size();

    if ((required_guardians == 3 && signed_guardians >= 2) || (required_guardians > 3 && signed_guardians >= 3))
    {
        recovers.modify(ritr, get_self(), [&](auto &item) {
            item.complete_timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
}

void guardians::claim(name user_account) {
    auto now = eosio::current_time_point().sec_since_epoch();
    auto gitr = guards.find(user_account.value);
    auto ritr = recovers.find(user_account.value);

    check(gitr != guards.end(), "no guardians for user " + user_account.to_string());
    check(ritr != recovers.end(), "no active recovery for user " + user_account.to_string());
    check(ritr -> complete_timestamp > 0, "recovery not complete for user " + user_account.to_string());
    check(ritr -> complete_timestamp + gitr->time_delay_sec <= now, 
        "Need to wait another " + 
        std::to_string(ritr -> complete_timestamp + gitr->time_delay_sec - now) + 
        " seconds until you can claim");
    
    recovers.erase(ritr);
    change_account_permission(user_account, ritr -> public_key);

}

void guardians::change_account_permission(name user_account, string public_key)
{
    authority newauth = guardian_key_authority(public_key);

    action(
        permission_level{user_account, "owner"_n},
        "eosio"_n, "updateauth"_n,
        make_tuple(
            user_account,
            "active"_n,
            "owner"_n,
            newauth))
        .send();
    action(
        permission_level{user_account, "owner"_n},
        "eosio"_n, "updateauth"_n,
        make_tuple(
            user_account,
            "owner"_n,
            ""_n,
            newauth))
        .send();
}

bool guardians::is_seeds_user(name account)
{
    DEFINE_USER_TABLE;
    DEFINE_USER_TABLE_MULTI_INDEX;
    user_tables users(contracts::accounts, contracts::accounts.value);

    auto uitr = users.find(account.value);
    return uitr != users.end();
}

authority guardians::guardian_key_authority(string key_str)
{
    const public_key key = string_to_public_key(key_str);

    authority ret_authority;

    key_weight kweight{
        .key = key,
        .weight = (uint16_t)1};

    eosio::permission_level perm(get_self(), "eosio.code"_n);

    permission_level_weight account_weight{
        .permission = perm,
        .weight = (uint16_t)1};

    ret_authority.threshold = 1;
    ret_authority.keys = {kweight};
    ret_authority.accounts = {account_weight};
    ret_authority.waits = {};

    return ret_authority;
}
