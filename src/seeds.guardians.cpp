#include <seeds.guardians.hpp>

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

void guardians::setup(name protectable_account, checksum256 invite_hash) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);

    if (aitr == accts.end()) {
        accts.emplace(protectable_account, get_self(), [&](auto &item) {
            item.account = protectable_account;
            item.status = "setup"_n;
            item.invite_hash = invite_hash;
        });
    } else {
        check(aitr->status == "cancelled", "cancel before init");
    }
}

void guardians::join(name guardian_account, name protectable_account, checksum256 invite_hash, string optional_public_key) {
    require_auth(guardian_account);

    if (!is_seeds_user(guardian_account)) {
        create_account(guardian_account, optional_public_key);
    }

    auto aitr = accts.find(protectable_account.value);

    check (aitr != accts.end(), "account not found");
    check (aitr->status == "setup"_n, "account is not setup stage");
    
    auto _invite_secret = invite_secret.extract_as_byte_array();
    checksum256 invite_hash = sha256((const char*)_invite_secret.data(), _invite_secret.size());

    check(aitr->invite_hash == invite_hash, "incorrect password");

    guardian_table guards(get_self(), protectable_account.value);
    
    check(guards.find(guardian_account.value) == guards.end());
    check(distance(guards.begin(), guards.end()) < max_guardians, "too many guardians");

    guards.emplace(get_self(), [&](auto &item) {
        item.account = guardian_account;
    });
}

void guardians::activate(name protectable_account) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);

    check (aitr != accts.end(), "account not found");
    check(aitr->status == "setup"_n, "account not setup stage ");

    guardian_table guards(get_self(), protectable_account.value);

    check(distance(guards.begin(), guards.end()) >= min_guardians, "not enough guardians");

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "active"_n;
    });
}

void guardians::pause(name protectable_account) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);

    check (aitr != accts.end(), "account not found");
    check (aitr->status == "active"_n, "account not active stage");

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "paused";
    });
}

void guardians::unpause(name protectable_account) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);

    check (aitr != accts.end(), "account not found");
    check (aitr->status == "paused"_n, "account not paused");

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "active";
    });
}

void guardians::deleteguards(name protectable_account) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);

    check(aitr != accts.end(), "account not found");
    check(aitr->status == "paused"_n, "account not paused");

    accts.modify(aitr, get_self(), [&](auto &item) {
        accts.status = "initial"_n;
    });

    auto gitr = guards.begin();
    while (gitr != guards.end()) {
        gitr = guards.erase(gitr);
    }
}

void guardians::newrecover(name guardian_account, name protectable_account, string new_public_key) {
    require_auth(guardian_account);

    guardian_table guards(get_self(), protectable_account.value);
    check(guards.find(guardian_account.value) != guards.end(), guardian_account.to_string() + " is not a guardian for " + protectable_account.to_string());

    recover_table recovers(get_self(), protectable_account.value);
    check(recovers.begin() == recovers.end(), " already in recovery ");

    auto aitr = accts.find(protectable_account.value);
    check(aitr->status == "active"_n, " not active status");

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "recovery";
        item.proposed_public_key = new_public_key
    });

    recovers.emplace(get_self(), [&](auto &item) {
        item.account = guardian_account;
    });
}

void guardians::confrecover(name guardian_account, name protectable_account, string new_public_key) {
    require_auth(guardian_account);

    guardian_table guards(get_self(), protectable_account.value);
    check(guards.find(guardian_account.value) != guards.end(), guardian_account.to_string() + " is not a guardian for " + protectable_account.to_string());

    recover_table recovers(get_self(), protectable_account.value);
    check(recovers.find(guardian_account.value) == recovers.end(), " already voted ");

    auto aitr = accts.find(protectable_account.value);
    check(aitr->status == "recovery"_n, " not in recovery process ");
    check(aitr->proposed_public_key == new_public_key, " different public key, first cancel and then make new proposal");

    recovers.emplace(get_self(), [&](auto &item) {
        item.account = guardian_account;
    });

    int guardians_number = distance(guards.begin(), guards.end());
    int approvals_number = distance(recovers.begin(), recovers.end());
    bool quorum_reached = (chosen - approved) * quorum_percent < 100;

    if (quorum_reached) {
        accts.modify(aitr, get_self(), [&](auto &item) {
            item.status = "complete"_n;
            item.complete_timestamp = eosio::current_time_point().sec_since_epoch();
        })
    }
}

void guardians::cancrecover(name guardian_account, name protectable_account) {
    require_auth(guardian_account);

    auto aitr = accts.find(protectable_account.value);
    check(aitr->status == "recovery"_n, "not in recovery");

    guardian_table guards(get_self(), protectable_account.value);
    check(guards.find(guardian_account.value) != guards.end(), "not a guardian account");

    recover_table recovers(get_self(), protectable_account.value);
    
    auto ritr = recovers.begin();
    while (ritr != recovers.end()) {
        ritr = recovers.erase(ritr);
    }
}

void guardians::stoprecover(name protectable_account) {
    require_auth(protectable_account);

    auto aitr = accts.find(protectable_account.value);
    check(aitr->status == "recovery"_n, "not in recovery");

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "active";
    });

    recover_table recover(get_self(), protectable_account);

    auto ritr = recover.begin();
    while(ritr != recover.end()) {
        ritr = recover.erase(ritr);
    }
}

void guardians::recover(name guardian_account, name protectable_account, string new_public_key) {
    require_auth(guardian_account);

    guardian_table guards(get_self(), protectable_account.value);

    check(guards.find(guardian_account.value) != guards.end(), "is not guardian for protectable account");

    recover_table recovers(get_self(), protectable_account.value);

    auto aitr = accts.find(protectable_account.value);

    check (aitr->status == "active"_n || aitr->status == "recovery"_n, "not possible in this stage");

    if (aitr->status == "active"_n) {
        accts.modify(aitr, get_self(), [&](auto &item) {
            item.status = "recovery";
            item.proposed_public_key = new_public_key;
        });

        recovers.emplace(get_self(), [&](auto &item) {
            item.account = guardian_account;
        });
    } else if (aitr->status == "recovery"_n) {
        if (recovers.find(guardian_account.value) == recovers.end()) {
            recovers.emplace(get_self(), [&](auto &item) {
                item.account = guardian_account;
            });

            int chosen = distance(guards.begin(), guards.end());
            int approved = restores(recovers.begin(), recovers.end());

            if ((chosen - approved) * quorum_percent  / 100 < 1) {
                accts.modify(aitr, get_self(), [&](auto &item) {
                    item.status = "complete"_n;
                    item.complete_timestamp = eosio::current_time_point().sec_since_epoch();
                });
            }
        } else {
            accts.modify(aitr, get_self(), [&](auto &item) {
                item.proposed_public_key = new_public_key;
            });

            auto ritr = recovers.begin();
            while (ritr != recovers.end()) {
                ritr = recovers.erase(ritr);
            }

            recovers.emplace(get_self(), [&](auto &item) {
                item.account = guardian_account;
            })
        }
    }
}

void guardians::claim(name protectable_account) {
    auto aitr = accts.find(protectable_account.value);
    check (aitr->status == "complete"_n, " account not ready to be claimed ");

    check (ritr->complete_timestamp + aitr->time_delay_sec <= eosio::current_time_point().sec_since_epoch(), " please wait..." );

    accts.modify(aitr, get_self(), [&](auto &item) {
        item.status = "active"_n;
    });

    auto ritr = recovers.begin();
    while (ritr != recovers.end()) {
        ritr = recovers.erase(ritr);
    }

    change_account_permission(protectable_account, aitr->proposed_public_key);
}

// void guardians::claim(name user_account) {
//     auto now = eosio::current_time_point().sec_since_epoch();
//     auto gitr = guards.find(user_account.value);
//     auto ritr = recovers.find(user_account.value);

//     check(gitr != guards.end(), "no guardians for user " + user_account.to_string());
//     check(ritr != recovers.end(), "no active recovery for user " + user_account.to_string());
//     check(ritr -> complete_timestamp > 0, "recovery not complete for user " + user_account.to_string());
//     check(ritr -> complete_timestamp + gitr->time_delay_sec <= now, 
//         "Need to wait another " + 
//         std::to_string(ritr -> complete_timestamp + gitr->time_delay_sec - now) + 
//         " seconds until you can claim");
    
//     recovers.erase(ritr);
//     change_account_permission(user_account, ritr -> public_key);

// }

// void guardians::recover(name guardian_account, name user_account, string new_public_key)
// {
//     require_auth(guardian_account);

//     auto gitr = guards.find(user_account.value);

//     check(gitr != guards.end(),
//           "account " + user_account.to_string() + " does not have guardians");

//     bool is_user_guardian = false;

//     for (std::size_t i = 0; i < gitr->guardians.size(); ++i)
//     {
//         if (gitr->guardians[i] == guardian_account)
//         {
//             is_user_guardian = true;
//         }
//     }

//     check(is_user_guardian == true,
//           "account " + guardian_account.to_string() +
//               " is not a guardian for " + user_account.to_string());

//     auto ritr = recovers.find(user_account.value);

//     if (ritr == recovers.end())
//     {
//         recovers.emplace(get_self(), [&](auto &item) {
//             item.account = user_account;
//             item.guardians = vector{guardian_account};
//             item.public_key = new_public_key;
//             item.complete_timestamp = 0;
//         });
//     }
//     else
//     {
//         if (ritr->public_key.compare(new_public_key) == 0)
//         {
//             bool is_guardian_recovering = false;

//             for (std::size_t i = 0; i < ritr->guardians.size(); ++i)
//             {
//                 if (ritr->guardians[i] == guardian_account)
//                 {
//                     is_guardian_recovering = true;
//                 }
//             }

//             check(is_guardian_recovering == false,
//                   "guardian " + guardian_account.to_string() + " already recovering " + user_account.to_string());

//             recovers.modify(ritr, get_self(), [&](auto &item) {
//                 item.guardians.push_back(guardian_account);
//             });
//         }
//         else
//         {
//             check(ritr -> complete_timestamp == 0, "recovery complete, waiting for claim - can't change key now");

//             recovers.modify(ritr, get_self(), [&](auto &item) {
//                 item.guardians = vector{guardian_account};
//                 item.public_key = new_public_key;
//                 item.complete_timestamp = 0;
//             });
//         }
//     }

//     int required_guardians = gitr->guardians.size();
//     int signed_guardians = ritr->guardians.size();

//     if ((required_guardians == 3 && signed_guardians >= 2) || (required_guardians > 3 && signed_guardians >= 3))
//     {
//         recovers.modify(ritr, get_self(), [&](auto &item) {
//             item.complete_timestamp = eosio::current_time_point().sec_since_epoch();
//         });
//     }
// }


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
