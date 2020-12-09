#include <seeds.guardians.hpp>

void guardians::reset()
{
    require_auth(get_self());

    auto pitr = protectables.begin();
    while (pitr != protectables.end()) {
        clear_guardians(pitr->account);
        clear_approvals(pitr->account);

        pitr = protectables.erase(pitr);
    }
}

void guardians::setup(name protectable_account, uint64_t claim_delay_sec, checksum256 guardian_invite_hash) {
    require_auth(protectable_account);

    auto pitr = protectables.find(protectable_account.value);

    if (pitr == protectables.end()) {
        protectables.emplace(get_self(), [&](auto &item) {
            item.account = protectable_account;
            item.current_status = status::setup;
            item.claim_delay_sec = claim_delay_sec;
            item.guardian_invite_hash = guardian_invite_hash;
        });
    } else {
        require_status(protectable_account, status::active);

        protectables.modify(pitr, get_self(), [&](auto &item) {
            item.current_status = status::setup;
            item.claim_delay_sec = claim_delay_sec;
            item.guardian_invite_hash = guardian_invite_hash;
        });
    }
}

void guardians::protect(name guardian_account, name protectable_account, checksum256 guardian_invite_secret) {
    require_auth(guardian_account);

    auto guardian_invite_secret_bytes = guardian_invite_secret.extract_as_byte_array();
    checksum256 actual_hash = sha256((const char*)guardian_invite_secret_bytes.data(), guardian_invite_secret_bytes.size());

    auto pitr = protectables.find(protectable_account.value);
    checksum256 expected_hash = pitr->guardian_invite_hash;
    check (expected_hash == actual_hash, "required secret for " + checksum_to_string(&expected_hash, sizeof(expected_hash)) + " but provided for " + checksum_to_string(&actual_hash, sizeof(actual_hash)));

    require_status(protectable_account, status::setup);

    add_guardian(protectable_account, guardian_account);
}

void guardians::protectby(name protectable_account, name guardian_account) {
    require_auth(get_self());

    auto pitr = protectables.find(protectable_account.value);

    if (pitr->current_status != status::setup) return;

    add_guardian(protectable_account, guardian_account);
}

void guardians::activate(name protectable_account) {
    require_auth(protectable_account);

    auto pitr = protectables.find(protectable_account.value);

    require_status(protectable_account, status::setup);

    guardian_tables guards(get_self(), protectable_account.value);
    int current_guardians = distance(guards.begin(), guards.end());
    check(current_guardians >= min_guardians, "required minimum number of " + to_string(min_guardians) + " but joined only " + to_string(current_guardians) + " guardians");

    protectables.modify(pitr, get_self(), [&](auto& item) {
        item.current_status = status::active;
    });
}

void guardians::pause(name protectable_account) {
    require_auth(protectable_account);

    require_status(protectable_account, status::active);

    auto pitr = protectables.find(protectable_account.value);
    protectables.modify(pitr, get_self(), [&](auto &item) {
        item.current_status = status::paused;
    });
}

void guardians::unpause(name protectable_account) {
    require_auth(protectable_account);

    require_status(protectable_account, status::paused);

    auto pitr = protectables.find(protectable_account.value);
    protectables.modify(pitr, get_self(), [&](auto& item) {
        item.current_status = status::active;
    });
}

void guardians::deleteguards(name protectable_account) {
    require_auth(protectable_account);

    auto pitr = protectables.find(protectable_account.value);

    require_status(protectable_account, status::paused);

    clear_guardians(protectable_account);
}

void guardians::newrecovery(name guardian_account, name protectable_account, string new_public_key) {
    require_auth(guardian_account);

    require_status(protectable_account, status::active);

    require_guardian(protectable_account, guardian_account);

    approve_recovery(protectable_account, guardian_account);

    auto pitr = protectables.find(protectable_account.value);
    protectables.modify(pitr, get_self(), [&](auto &item) {
        item.current_status = status::recovery;
        item.proposed_public_key = new_public_key;
    });
}

void guardians::yesrecovery(name guardian_account, name protectable_account) {
    require_auth(guardian_account);

    require_status(protectable_account, status::recovery);

    require_guardian(protectable_account, guardian_account);

    approve_recovery(protectable_account, guardian_account);

    guardian_tables guards(get_self(), protectable_account.value);
    int guardians_number = distance(guards.begin(), guards.end());

    approval_tables approvals(get_self(), protectable_account.value);
    int approvals_number = distance(approvals.begin(), approvals.end());

    bool quorum_reached = (guardians_number - approvals_number) * quorum_percent < 100;

    if (quorum_reached) {
        auto pitr = protectables.find(protectable_account.value);
        protectables.modify(pitr, get_self(), [&](auto &item) {
            item.current_status = status::complete;
            item.complete_timestamp = eosio::current_time_point().sec_since_epoch();
        });
    }
}

void guardians::norecovery(name guardian_account, name protectable_account) {
    require_auth(guardian_account);

    require_status(protectable_account, status::recovery);

    require_guardian(protectable_account, guardian_account);

    clear_approvals(protectable_account);

    auto pitr = protectables.find(protectable_account.value);
    protectables.modify(pitr, get_self(), [&](auto item) {
        item.current_status = status::active;
    });
}

void guardians::stoprecovery(name protectable_account) {
    require_auth(protectable_account);

    require_status(protectable_account, status::recovery);

    clear_approvals(protectable_account);

    auto pitr = protectables.find(protectable_account.value);
    protectables.modify(pitr, get_self(), [&](auto item) {
        item.current_status = status::active;
    });
}

void guardians::claim(name protectable_account) {
    require_auth(get_self());

    require_status(protectable_account, status::complete);

    auto pitr = protectables.find(protectable_account.value);

    auto now = eosio::current_time_point().sec_since_epoch();
    auto then = pitr->complete_timestamp + pitr->claim_delay_sec;

    check (then <= now, " need to wait another " + to_string(then - now) + " seconds until account can be claimed");

    change_account_permission(protectable_account, pitr->proposed_public_key);

    protectables.modify(pitr, get_self(), [&](auto &item) {
        item.current_status = status::active;
    });

    clear_approvals(protectable_account);
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
    const abieos::public_key key = string_to_public_key(key_str);

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

void guardians::clear_guardians(name protectable_account) {
    guardian_tables guards(get_self(), protectable_account.value);

    auto gitr = guards.begin();
    while (gitr != guards.end()) {
        gitr = guards.erase(gitr);
    }
}

void guardians::clear_approvals(name protectable_account) {
    approval_tables approvals(get_self(), protectable_account.value);
    auto ritr = approvals.begin();
    while (ritr != approvals.end()) {
        ritr = approvals.erase(ritr);
    }    
}

void guardians::add_guardian(name protectable_account, name guardian_account) {
    guardian_tables guards(get_self(), protectable_account.value);
    
    check (guards.find(guardian_account.value) == guards.end(), "already guardian for protectable account");
    check (distance(guards.begin(), guards.end()) < max_guardians, "maximum guardians reached for protectable account");

    guards.emplace(get_self(), [&](auto &item) {
        item.account = guardian_account;
    });    
}

void guardians::approve_recovery(name protectable_account, name guardian_account) {
    approval_tables approvals(get_self(), protectable_account.value);

    check (approvals.find(guardian_account.value) == approvals.end(), 
        "recovery of " + protectable_account.to_string() + " already approved by " + guardian_account.to_string());

    approvals.emplace(get_self(), [&](auto &item) {
        item.account = guardian_account;
    });
}

void guardians::require_status(name protectable_account, status required_status) {
    auto pitr = protectables.find(protectable_account.value);

    check (pitr != protectables.end(), 
        "required status of " + protectable_account.to_string() + " is " + status_to_string(required_status) + " but account has not been initialized yet");
    check (pitr->current_status == required_status, 
        "required status of " + protectable_account.to_string() + " is " + status_to_string(required_status) + " but current status is " + status_to_string(pitr->current_status));
}

void guardians::require_guardian(name protectable_account, name guardian_account) {
    guardian_tables guards(get_self(), protectable_account.value);
    check (guards.find(guardian_account.value) != guards.end(), guardian_account.to_string() + " is not a guardian for " + protectable_account.to_string());
}

string guardians::status_to_string(status given_status) {
    switch (given_status) {
        case status::setup:
            return "setup";
        case status::active:
            return "active";
        case status::paused:
            return "paused";
        case status::recovery:
            return "recovery";
        case status::complete:
            return "complete";
    }
}

string guardians::checksum_to_string(const checksum256* d , uint32_t s) {
  std::string r;
  const char* to_hex="0123456789abcdef";
  uint8_t* c = (uint8_t*)d;
  for( uint32_t i = 0; i < s; ++i ) {
    (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
  }
  return r;
}