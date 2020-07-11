#include <seeds.bioregion.hpp>
#include <eosio/system.hpp>
#include <string_view>
#include <string>


ACTION bioregion::reset() {
    require_auth(_self);

        // roles_tables roles(get_self(), name("testbr.bdc").value);
        // auto ritr = roles.begin();
        // while(ritr != roles.end()) {
        //     ritr = roles.erase(ritr);
        // }

    auto itr = bioregions.begin();
    while(itr != bioregions.end()) {
        roles_tables roles(get_self(), itr->id.value);
        auto ritr = roles.begin();
        while(ritr != roles.end()) {
            ritr = roles.erase(ritr);
        }
        itr = bioregions.erase(itr);
    }

    auto mitr = members.begin();
    while(mitr != members.end()) {
        mitr = members.erase(mitr);
    }

    auto sitr = sponsors.begin();
    while(sitr != sponsors.end()){
        sitr = sponsors.erase(sitr);
    }
}

void bioregion::auth_founder(name bioregion, name founder) {
    require_auth(founder);
    auto itr = bioregions.get(bioregion.value, "The bioregion does not exist.");
    check(itr.founder == founder, "Only the bioregion's founder can do that.");
    check_user(founder);
}

bool bioregion::is_member(name bioregion, name account) {
    auto mitr = members.find(account.value);
    if (mitr != members.end()) {
        return mitr->bioregion == bioregion;
    }
    return false;
}

bool bioregion::is_admin(name bioregion, name account) {
    roles_tables roles(get_self(), bioregion.value);
    auto ritr = roles.find(account.value);
    if (ritr != roles.end()) {
        return ritr->role == admin_role || ritr->role == founder_role;
    }
    return false;
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

ACTION bioregion::create(
    name founder, 
    name bioaccount, 
    string description, 
    string locationJson, 
    float latitude, 
    float longitude, 
    string publicKey) 
{
    require_auth(founder);
    check_user(founder);

    //string acct_string = bioaccount.to_string();
    check(bioaccount.suffix().to_string() == "bdc", "Bioregion name must end in '.bdc' Your suffix: " + bioaccount.suffix().to_string());

    auto sitr = sponsors.find(founder.value);
    check(sitr != sponsors.end(), "The sponsor account does not have a balance entry in this contract.");

    auto feeparam = config.get("bio.fee"_n.value, "The bio.fee parameter has not been initialized yet.");
    asset quantity(feeparam.value, seeds_symbol);

    check(sitr->balance >= quantity, "The user does not have enough credit to create an organization" + sitr->balance.to_string() + " min: "+quantity.to_string());

    auto bitr = bioregions.find(bioaccount.value);
    check(bitr == bioregions.end(), "This bioregion already exists.");
    
    auto uitr = users.find(founder.value);
    check(uitr != users.end(), "Sponsor is not a Seeds account.");

    create_telos_account(founder, bioaccount, publicKey);

    sponsors.modify(sitr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;           
    });

    bioregions.emplace(_self, [&](auto & item) {
        item.id = bioaccount;
        item.founder = founder;
        item.status = "inactive"_n;
        item.description = description;
        item.locationjson = locationJson;
        item.latitude = latitude;
        item.longitude = longitude;
        item.members_count = 0;
    });

    join(bioaccount, founder);

    roles_tables roles(get_self(), bioaccount.value);
    roles.emplace(_self, [&](auto & item) {
        item.account = founder;
        item.role = founder_role;
    });

}

ACTION bioregion::join(name bioregion, name account) {
    require_auth(account);
    check_user(account);

    auto bitr = bioregions.find(bioregion.value);
    check(bitr != bioregions.end(), "no bioregion");

    auto mitr = members.find(account.value);

    if (mitr != members.end()) {
        check(false, "user exists TODO allow users to switch");
    } else {
        members.emplace(_self, [&](auto & item) {
            item.bioregion = bioregion;
            item.account = account;
        });
        size_change(bioregion, 1);
    }

}

ACTION bioregion::addrole(name bioregion, name admin, name account, name role) {
    auth_founder(bioregion, admin);
    check_user(account);
    check(role == admin_role, "invalid role");
    check(is_member(bioregion, account), "account is not a member, can't have a role");

    roles_tables roles(get_self(), bioregion.value);

    auto ritr = roles.find(account.value);
    if (ritr != roles.end()) {
        roles.modify(ritr, _self, [&](auto & item) {
            item.account = account;
            item.role = role;
        });    
    } else {
        roles.emplace(_self, [&](auto & item) {
            item.account = account;
            item.role = role;
        });
    }

}

ACTION bioregion::removerole(name bioregion, name founder, name account) {
    auth_founder(bioregion, founder);
    delete_role(bioregion, account);
}

ACTION bioregion::leaverole(name bioregion, name account) {
    require_auth(account);
    delete_role(bioregion, account);
}

void bioregion::delete_role(name bioregion, name account) {
    roles_tables roles(get_self(), bioregion.value);
    auto ritr = roles.find(account.value);
    check(ritr != roles.end(), "no role");
    roles.erase(ritr);
}

ACTION bioregion::removemember(name bioregion, name admin, name account) {
    require_auth(admin);
    is_admin(bioregion, admin);

    auto bitr = bioregions.find(bioregion.value);
    check(bitr -> founder != account, "Change the organization's owner before removing this account.");

    remove_member(account);
}

ACTION bioregion::leave(name bioregion, name account) {
    require_auth(account);
    remove_member(account);
}

void bioregion::remove_member(name account) {
    auto mitr = members.find(account.value);
    
    check(mitr != members.end(), "member not found");

    size_change(mitr->bioregion, -1);

    members.erase(mitr);

}

ACTION bioregion::setfounder(name bioregion, name founder, name new_founder) {
    auth_founder(bioregion, founder);
    check(founder.value != new_founder.value, "need to set new account");

    delete_role(bioregion, founder);
    addrole(bioregion, founder, founder, "admin"_n);

    auto bitr = bioregions.find(bioregion.value);
    check(bitr != bioregions.end(), "The bioregion does not exist.");
    bioregions.modify(bitr, _self, [&](auto& item) {
      item.founder = new_founder;
    });
    

}

void bioregion::create_telos_account(name sponsor, name orgaccount, string publicKey) 
{
    action(
        permission_level{contracts::onboarding, "createbio"_n},
        contracts::onboarding, "createbio"_n,
        make_tuple(sponsor, orgaccount, publicKey)
    ).send();
}

void bioregion::size_change(name bioregion, int delta) {
    auto bitr = bioregions.find(bioregion.value);

    check(bitr != bioregions.end(), "bioregion not found");
  
    uint64_t newsize = bitr->members_count + delta; 
    if (delta < 0) {
      if (bitr->members_count < -delta) {
        newsize = 0;
      }
    }
    bioregions.modify(bitr, _self, [&](auto& item) {
      item.members_count = newsize;
    });
}

