#include <seeds.region.hpp>
#include <eosio/system.hpp>
#include <string_view>
#include <string>


ACTION region::reset() {
    require_auth(_self);

    auto itr = regions.begin();
    while(itr != regions.end()) {
        roles_tables roles(get_self(), itr->id.value);
        auto ritr = roles.begin();
        while(ritr != roles.end()) {
            ritr = roles.erase(ritr);
        }
        itr = regions.erase(itr);
    }

    auto mitr = members.begin();
    while(mitr != members.end()) {
        mitr = members.erase(mitr);
    }

    auto sitr = sponsors.begin();
    while(sitr != sponsors.end()){
        sitr = sponsors.erase(sitr);
    }

    auto ditr = regiondelays.begin();
    while(ditr != regiondelays.end()) {
        ditr = regiondelays.erase(ditr);
    }
}

void region::auth_founder(name region, name founder) {
    require_auth(founder);
    auto itr = regions.get(region.value, "The region does not exist.");
    check(itr.founder == founder, "Only the region's founder can do that.");
    check_user(founder);
}

bool region::is_member(name region, name account) {
    auto mitr = members.find(account.value);
    if (mitr != members.end()) {
        return mitr->region == region;
    }
    return false;
}

bool region::is_admin(name region, name account) {
    roles_tables roles(get_self(), region.value);
    auto ritr = roles.find(account.value);
    if (ritr != roles.end()) {
        return ritr->role == admin_role || ritr->role == founder_role;
    }
    return false;
}


void region::init_balance(name account) {
    auto itr = sponsors.find(account.value);
    if(itr == sponsors.end()){
        sponsors.emplace(_self, [&](auto & nbalance) {
            nbalance.account = account;
            nbalance.balance = asset(0, seeds_symbol);
        });
    }
}

void region::check_user(name account) {
    auto uitr = users.find(account.value);
    check(uitr != users.end(), "region: no user.");
}

void region::deposit(name from, name to, asset quantity, string memo) {
    if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
        to  ==  get_self() &&                     // to here
        quantity.symbol == seeds_symbol) {        // SEEDS symbol

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

ACTION region::create(
    name founder, 
    name rgnaccount, 
    string description, 
    string locationJson, 
    float latitude, 
    float longitude, 
    string publicKey) 
{
    require_auth(founder);
    check_user(founder);

    //string acct_string = rgnaccount.to_string();
    check(rgnaccount.suffix().to_string() == "rgn", "region name must end in '.rgn' Your suffix: " + rgnaccount.suffix().to_string());

    auto sitr = sponsors.find(founder.value);
    check(sitr != sponsors.end(), "The founder account does not have a balance entry in this contract.");

    auto feeparam = config.get("region.fee"_n.value, "The region.fee parameter has not been initialized yet.");
    asset quantity(feeparam.value, seeds_symbol);

    check(sitr->balance >= quantity, "The user does not have enough credit to create an organization" + sitr->balance.to_string() + " min: "+quantity.to_string());

    auto bitr = regions.find(rgnaccount.value);
    check(bitr == regions.end(), "This region already exists.");
    
    auto uitr = users.find(founder.value);
    check(uitr != users.end(), "Founder is not a Seeds account.");

    auto mitr = members.find(founder.value);
    check(mitr == members.end(), "Founder is part of another region. Leave the other region first.");

    create_telos_account(founder, rgnaccount, publicKey);

    sponsors.modify(sitr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;           
    });

    regions.emplace(_self, [&](auto & item) {
        item.id = rgnaccount;
        item.founder = founder;
        item.status = status_inactive;
        item.description = description;
        item.locationjson = locationJson;
        item.latitude = latitude;
        item.longitude = longitude;
        item.members_count = 0;
    });

    join(rgnaccount, founder);

    roles_tables roles(get_self(), rgnaccount.value);
    roles.emplace(_self, [&](auto & item) {
        item.account = founder;
        item.role = founder_role;
    });

}

ACTION region::join(name region, name account) {
    require_auth(account);
    check_user(account);

    auto ritr = regions.find(region.value);
    check(ritr != regions.end(), "no region");

    auto mitr = members.find(account.value);

    check(mitr == members.end(), "user already belongs to a region");

    uint64_t now = eosio::current_time_point().sec_since_epoch();

    auto ditr = regiondelays.find(account.value);
    if (ditr == regiondelays.end()) {
        regiondelays.emplace(_self, [&](auto & item){
            item.account = account;
            item.apply_vote_delay = false;
            item.joined_date_timestamp = now;
        });
    } else {
        check(ditr -> joined_date_timestamp < now - (utils::moon_cycle * config_float_get("rgn.vote.del"_n)), "user needs to wait until the delay ends");
        regiondelays.modify(ditr, _self, [&](auto & item){
            item.apply_vote_delay = true;
            item.joined_date_timestamp = now;
        });
    }

    members.emplace(_self, [&](auto & item) {
        item.region = region;
        item.account = account;
    });
    size_change(region, 1);

}

ACTION region::addrole(name region, name admin, name account, name role) {
    auth_founder(region, admin);
    check_user(account);
    check(role == admin_role, "invalid role");
    check(is_member(region, account), "account is not a member, can't have a role");

    roles_tables roles(get_self(), region.value);

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

ACTION region::removerole(name region, name founder, name account) {
    auth_founder(region, founder);
    delete_role(region, account);
}

ACTION region::leaverole(name region, name account) {
    require_auth(account);
    delete_role(region, account);
}

void region::delete_role(name region, name account) {
    roles_tables roles(get_self(), region.value);
    auto ritr = roles.find(account.value);
    check(ritr != roles.end(), "no role");
    roles.erase(ritr);
}

ACTION region::removemember(name region, name admin, name account) {
    require_auth(admin);
    is_admin(region, admin);

    auto ritr = regions.find(region.value);
    check(ritr -> founder != account, "Change the organization's owner before removing this account.");

    remove_member(account);
}

ACTION region::leave(name region, name account) {
    require_auth(account);
    remove_member(account);
}

void region::remove_member(name account) {
    auto mitr = members.find(account.value);
    
    check(mitr != members.end(), "member not found");

    roles_tables roles(get_self(), mitr -> region.value);
    auto ritr = roles.find(account.value);
    
    if (ritr != roles.end()) {
        roles.erase(ritr);
    }

    size_change(mitr->region, -1);

    members.erase(mitr);
}

ACTION region::setfounder(name region, name founder, name new_founder) {
    auth_founder(region, founder);
    check(founder.value != new_founder.value, "need to set new account");

    delete_role(region, founder);
    addrole(region, founder, founder, "admin"_n);

    auto ritr = regions.find(region.value);
    check(ritr != regions.end(), "The region does not exist.");
    regions.modify(ritr, _self, [&](auto& item) {
      item.founder = new_founder;
    });
}

ACTION region::removebr(name region) {
    require_auth(get_self());
    auto bitr = regions.find(region.value);
    check(bitr != regions.end(), "The region does not exist.");
    regions.erase(bitr);

    roles_tables roles(get_self(), region.value);
    auto ritr = roles.begin();
    while(ritr != roles.end()) {
        ritr = roles.erase(ritr);
    }
    auto rgnmembers = members.get_index<"byregion"_n>();
    uint64_t current_user = 0;

    auto mitr = rgnmembers.find(region.value);
    while (mitr != rgnmembers.end() && mitr->region.value == region.value) {
        mitr = rgnmembers.erase(mitr);
    }
}

void region::create_telos_account(name sponsor, name orgaccount, string publicKey) 
{
    action(
        permission_level{contracts::onboarding, "active"_n},
        contracts::onboarding, "createregion"_n,
        make_tuple(sponsor, orgaccount, publicKey)
    ).send();
}

void region::size_change(name region, int delta) {
    auto bitr = regions.find(region.value);

    check(bitr != regions.end(), "region not found");
  
    uint64_t newsize = ritr->members_count + delta; 
    if (delta < 0) {
      if (ritr->members_count < -delta) {
        newsize = 0;
      }
    }
    regions.modify(bitr, _self, [&](auto& item) {
      item.members_count = newsize;
      if (item.members_count >= active_cutoff) {
        item.status = status_active;
      } else {
        item.status = status_inactive;
      }
    });
}

double region::config_float_get(name key) {
  auto citr = configfloat.find(key.value);
  if (citr == configfloat.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}

uint64_t region::config_get(name key) {
  auto citr = config.find(key.value);
  if (citr == config.end()) { 
    check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
  }
  return citr->value;
}
