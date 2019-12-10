#include <seeds.organization.hpp>
#include <eosio/system.hpp>



/*
ACTION organization::addorg(name org, name owner) {
    organizations.emplace(_self, [&](auto & new_org) {
        new_org.org_name = org;
        new_org.owner = owner;
        new_org.status = 0;
        new_org.voice = 0;
        new_org.regen = 0;
    });
}
*/


void organization::check_owner(name organization, name owner) {
    require_auth(owner);
    auto itr = organizations.get(organization.value, "The organization does not exist.");
    check(itr.owner == owner, "Only the organization's owner can do that.");
}


uint64_t organization::getregenp(name account) {
    // this function will contain the regen points logic
    return 10;
}


ACTION organization::reset() {
    require_auth(_self);

    auto itr = organizations.begin();
    while(itr != organizations.end()) {
        name org = itr -> org_name;
        members_tables members(get_self(), org.value);
        auto mitr = members.begin();
        while(mitr != members.end()) {
            mitr = members.erase(mitr);
        }
        itr = organizations.erase(itr);
    }
}


ACTION organization::addmember(name organization, name owner, name account, name role) {
    check_owner(organization, owner);
    
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
    auto mitr = members.get(account.value, "The member does not exist.");
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

    auto itr = organizations.find(organization.value);
    organizations.modify(itr, _self, [&](auto & morg) {
        morg.owner = account;
    });
}


ACTION organization::addregen(name organization, name account) {
    require_auth(account); // is that required ???
    
    auto itr = organizations.find(organization.value);
    check(itr != organizations.end(), "Organization does not exist.");
    organizations.modify(itr, _self, [&](auto & morg) {
        morg.regen += getregenp(account);
    });
}


ACTION organization::subregen(name organization, name account) {
    require_auth(account); // is that required ???
    
    auto itr = organizations.find(organization.value);
    check(itr != organizations.end(), "Organization does not exist.");
    organizations.modify(itr, _self, [&](auto & morg) {
        morg.regen -= getregenp(account);
    });
}


EOSIO_DISPATCH(organization, (reset)(addmember)(removemember)(changerole)(changeowner)(addregen)(subregen));


