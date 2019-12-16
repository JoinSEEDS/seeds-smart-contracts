#include <seeds.organization.hpp>
#include <eosio/system.hpp>


void organization::check_owner(name organization, name owner) {
    require_auth(owner);
    auto itr = organizations.get(organization.value, "The organization does not exist.");
    check(itr.owner == owner, "Only the organization's owner can do that.");
    check_user(owner);
}


void organization::init_balance(name account) {
    auto itr = balances.find(account.value);
    if(itr == balances.end()){
        balances.emplace(_self, [&](auto & nbalance) {
            nbalance.account = account;
            nbalance.balance = asset(0, seeds_symbol);
        });
    }
}


void organization::check_user(name account) {
    auto uitr = users.find(account.value);
    check(uitr != users.end(), "organization: no user.");
}


int64_t organization::getregenp(name account) {
    auto itr = users.find(account.value);
    return 1 * itr -> reputation; // suposing a 4 decimals reputation allocated in a uint64_t variable
}


void organization::check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == seeds_symbol, "invalid asset");
}


void organization::deposit(name from, name to, asset quantity, string memo) {
    if(to == _self){
        check_user(from);

        init_balance(from);
        init_balance(to);

        auto fitr = balances.find(from.value);
        balances.modify(fitr, _self, [&](auto & mbalance) {
            mbalance.balance += quantity;
        });

        auto titr = balances.find(to.value);
        balances.modify(titr, _self, [&](auto & mbalance) {
            mbalance.balance += quantity;
        });
    }
}


// this function is just for testing
ACTION organization::createbalance(name user, asset quantity) {
    
    balances.emplace(_self, [&](auto & nbalance) {
        nbalance.account = user;
        nbalance.balance = quantity;
    });
}


ACTION organization::reset() {
    require_auth(_self);

    auto itr = organizations.begin();
    while(itr != organizations.end()) {
        name org = itr -> org_name;
        members_tables members(get_self(), org.value);
        vote_tables votes(get_self(), org.value);

        auto mitr = members.begin();
        while(mitr != members.end()) {
            mitr = members.erase(mitr);
        }

        auto vitr = votes.begin();
        while(vitr != votes.end()){
            vitr = votes.erase(vitr);
        }

        itr = organizations.erase(itr);
    }

    auto bitr = balances.begin();
    while(bitr != balances.end()){
        bitr = balances.erase(bitr);
    }
}


ACTION organization::create(name orgname, name sponsor) {
    require_auth(sponsor); // should the sponsor give the authorization? or it should be the contract itself?

    auto itr = balances.find(sponsor.value);
    check(itr != balances.end(), "The sponsor account does not have a balance entry in this contract.");

    auto feeparam = config.get(fee.value, "The fee parameter has not been initialized yet.");
    asset quantity(feeparam.value, seeds_symbol);

    check(itr -> balance >= quantity, "The user does not have enough credit to create an organization");

    auto orgitr = organizations.find(orgname.value);
    check(orgitr == organizations.end(), "This organization already exists.");
    
    balances.modify(itr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;           
    });

    organizations.emplace(_self, [&](auto & norg) {
        norg.org_name = orgname;
        norg.owner = sponsor;
        norg.fee = quantity;
    });

    addmember(orgname, sponsor, sponsor, ""_n);
}


ACTION organization::destroy(name organization, name owner) {
    check_owner(organization, owner);

    auto orgitr = organizations.find(organization.value);
    check(orgitr != organizations.end(), "organization: the organization does not exist.");

    auto bitr = balances.find(owner.value);
    balances.modify(bitr, _self, [&](auto & mbalance) {
        mbalance.balance += orgitr -> fee;
    });

    members_tables members(get_self(), organization.value);
    auto mitr = members.begin();
    while(mitr != members.end()){
        mitr = members.erase(mitr);
    }
    
    auto org = organizations.find(organization.value);
    organizations.erase(org);

    // refund(owner, fee); this method could be called if we want to refund as soon as the user destroys an organization
}


ACTION organization::refund(name beneficiary, asset quantity) {
    require_auth(beneficiary);
    
    check_asset(quantity);

    auto itr = balances.find(beneficiary.value);
    check(itr != balances.end(), "organization: user has no entry in the balance table.");
    check(itr -> balance >= quantity, "organization: user has not enough balance.");

    string memo = "refund";

    action(
        permission_level(_self, "active"_n),
        contracts::token,
        "transfer"_n,
        std::make_tuple(_self, beneficiary, quantity, memo)
    ).send();

    auto bitr = balances.find(_self.value);
    balances.modify(bitr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;
    });

    balances.modify(itr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;
    });
}


ACTION organization::addmember(name organization, name owner, name account, name role) {
    check_owner(organization, owner);
    check_user(account);
    
    members_tables members(get_self(), organization.value);
    members.emplace(_self, [&](auto & nmember) {
        nmember.account = account;
        nmember.role = role;
    });
}


ACTION organization::removemember(name organization, name owner, name account) {
    check_owner(organization, owner);

    auto itr = organizations.find(organization.value);
    check(itr -> owner != account, "Change the organization's owner before removing this account.");

    members_tables members(get_self(), organization.value);
    auto mitr = members.find(account.value);
    members.erase(mitr);
}


ACTION organization::changerole(name organization, name owner, name account, name new_role) {
    check_owner(organization, owner);

    members_tables members(get_self(), organization.value);
    
    auto mitr = members.find(account.value);
    check(mitr != members.end(), "Member does not exist.");

    members.modify(mitr, _self, [&](auto & mmember) {
        mmember.role = new_role;
    });
}


ACTION organization::changeowner(name organization, name owner, name account) {
    check_owner(organization, owner);
    check_user(account);

    auto bitr = balances.find(owner.value);
    auto aitr = balances.find(account.value);
    auto orgitr = organizations.find(organization.value);

    check(aitr != balances.end(), "organization: the account does not have an entry in the balance table.");
    check(aitr -> balance >= orgitr -> fee, "organization: the account does not hace enough balance.");

    balances.modify(bitr, _self, [&](auto & mbalace) {
        mbalace.balance += orgitr ->fee;
    });

    balances.modify(aitr, _self, [&](auto & mbalace) {
        mbalace.balance -= orgitr -> fee;
    });

    organizations.modify(orgitr, _self, [&](auto & morg) {
        morg.owner = account;
    });
}


void organization::vote(name organization, name account, int64_t regen) {
    vote_tables votes(get_self(), organization.value);

    auto itr = organizations.find(organization.value);
    check(itr != organizations.end(), "Organization does not exist.");
    
    organizations.modify(itr, _self, [&](auto & morg) {
        morg.regen += regen;
    });

    votes.emplace(_self, [&](auto & nvote) {
        nvote.account = account;
        nvote.timestamp = eosio::current_time_point().sec_since_epoch();
        nvote.regen_points = regen;
    });
}


ACTION organization::addregen(name organization, name account) {
    require_auth(account); // is that required ???
    check_user(account);

    vote_tables votes(get_self(), organization.value);

    auto vitr = votes.find(account.value);
    
    if(vitr != votes.end()){
        auto itr = organizations.find(organization.value);
        check(itr != organizations.end(), "Organization does not exist.");
        organizations.modify(itr, _self, [&](auto & morg) {
            morg.regen -= vitr -> regen_points;
        });
        votes.erase(vitr);
    }
    
    vote(organization, account, getregenp(account));
}


ACTION organization::subregen(name organization, name account) {
    require_auth(account); // is that required ???
    check_user(account);

    vote_tables votes(get_self(), organization.value);

    auto vitr = votes.find(account.value);
    
    if(vitr != votes.end()){
        auto itr = organizations.find(organization.value);
        check(itr != organizations.end(), "Organization does not exist.");
        organizations.modify(itr, _self, [&](auto & morg) {
            morg.regen -= vitr -> regen_points;
        });
        votes.erase(vitr);
    }
    
    vote(organization, account, -1 * getregenp(account));
}


