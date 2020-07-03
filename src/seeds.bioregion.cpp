#include <seeds.bioregion.hpp>
#include <eosio/system.hpp>

void bioregion::check_owner(name bioregion, name owner) {
    require_auth(owner);
    auto itr = bioregions.get(bioregion.value, "The bioregion does not exist.");
    check(itr.founder == owner, "Only the bioregion's owner can do that.");
    check_user(owner);
}


void bioregion::init_balance(name account) {
    auto itr = sponsors.find(account.value);
    if(itr == sponsors.end()){
        sponsors.emplace(_self, [&](auto & nbalance) {
            nbalance.account = account;
            nbalance.balance = asset(0, seeds_symbol);
        });
    }
}

void bioregion::check_user(name account) {
    auto uitr = users.find(account.value);
    check(uitr != users.end(), "bioregion: no user.");
}

void bioregion::deposit(name from, name to, asset quantity, string memo) {
    if(to == _self){
        utils::check_asset(quantity);
        check_user(from);

        init_balance(from);
        init_balance(to);

        auto fitr = sponsors.find(from.value);
        sponsors.modify(fitr, _self, [&](auto & mbalance) {
            mbalance.balance += quantity;
        });

        auto titr = sponsors.find(to.value);
        sponsors.modify(titr, _self, [&](auto & mbalance) {
            mbalance.balance += quantity;
        });
    }
}



ACTION bioregion::reset() {
    require_auth(_self);

    auto itr = bioregions.begin();
    while(itr != bioregions.end()) {
        name bio = itr -> id;
        members_tables members(get_self(), bio.value);

        auto mitr = members.begin();
        while(mitr != members.end()) {
            mitr = members.erase(mitr);
        }

        itr = bioregions.erase(itr);
    }

    auto sitr = sponsors.begin();
    while(sitr != sponsors.end()){
        sitr = sponsors.erase(sitr);
    }
}




