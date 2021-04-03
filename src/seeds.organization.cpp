#include <seeds.organization.hpp>
#include <eosio/system.hpp>


// uint64_t organization::get_config(name key) {
//     auto citr = config.find(key.value);
//     check(citr != config.end(), ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
//     return citr -> value;
// }

void organization::check_owner(name organization, name owner) {
    require_auth(owner);
    auto itr = organizations.get(organization.value, "The organization does not exist.");
    check(itr.owner == owner, "Only the organization's owner can do that.");
    check_user(owner);
}

void organization::init_balance(name account) {
    auto itr = sponsors.find(account.value);
    if(itr == sponsors.end()){
        sponsors.emplace(_self, [&](auto & nbalance) {
            nbalance.account = account;
            nbalance.balance = asset(0, seeds_symbol);
        });
    }
}

void organization::check_user(name account) {
    auto uitr = users.find(account.value);
    check(uitr != users.end(), "organisation: no user.");
}

int64_t organization::getregenp(name account) {
    auto itr = users.find(account.value);
    return itr -> reputation; // suposing a 4 decimals reputation allocated in a uint64_t variable
}

uint64_t organization::get_beginning_of_day_in_seconds() {
    auto sec = eosio::current_time_point().sec_since_epoch();
    auto date = eosio::time_point_sec(sec / 86400 * 86400);
    return date.utc_seconds;
}

uint64_t organization::get_size(name id) {
    auto itr = sizes.find(id.value);
    if (itr == sizes.end()) {
        return 0;
    }
    return itr -> size;
}

void organization::increase_size_by_one(name id) {
    auto itr = sizes.find(id.value);
    if (itr != sizes.end()) {
        sizes.modify(itr, _self, [&](auto & s){
            s.size += 1;
        });
    } else {
        sizes.emplace(_self, [&](auto & s){
            s.id = id;
            s.size = 1;
        });
    }
}

void organization::decrease_size_by_one(name id) {
    auto itr = sizes.find(id.value);
    if (itr != sizes.end()) {
        sizes.modify(itr, _self, [&](auto & s){
            if (s.size > 0) {
                s.size -= 1;
            }
        });
    }
}

uint64_t organization::config_get (name key) {
    auto citr = config.find(key.value);
        if (citr == config.end()) { 
        // only create the error message string in error case for efficiency
        eosio::check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
    }
    return citr->value;
}


void organization::deposit(name from, name to, asset quantity, string memo) {
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


// this function is just for testing
/*
ACTION organization::createbalance(name user, asset quantity) {
    
    sponsors.emplace(_self, [&](auto & nbalance) {
        nbalance.account = user;
        nbalance.balance = quantity;
    });
}
*/


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

    auto aitr = apps.begin();
    while(aitr != apps.end()) {
        dau_tables daus(get_self(), aitr->app_name.value);
        dau_history_tables dau_history(get_self(), aitr->app_name.value);
        auto dauitr = daus.begin();
        while (dauitr != daus.end()) {
            dauitr = daus.erase(dauitr);
        }
        auto dau_history_itr = dau_history.begin();
        while (dau_history_itr != dau_history.end()) {
            dau_history_itr = dau_history.erase(dau_history_itr);
        }
        aitr = apps.erase(aitr);
    }

    auto bitr = sponsors.begin();
    while(bitr != sponsors.end()){
        bitr = sponsors.erase(bitr);
    }

    auto sitr = sizes.begin();
    while (sitr != sizes.end()) {
        sitr = sizes.erase(sitr);
    }

    auto avgitr = avgvotes.begin();
    while (avgitr != avgvotes.end()) {
        avgitr = avgvotes.erase(avgitr);
    }

    auto regenitr = regenscores.begin();
    while (regenitr != regenscores.end()) {
        regenitr = regenscores.erase(regenitr);
    }

    auto cbsitr = cbsorgs.begin();
    while (cbsitr != cbsorgs.end()) {
        cbsitr = cbsorgs.erase(cbsitr);
    }
}


ACTION organization::create(name sponsor, name orgaccount, string orgfullname, string publicKey) 
{
    require_auth(sponsor);

    auto sitr = sponsors.find(sponsor.value);
    check(sitr != sponsors.end(), "The sponsor account does not have a balance entry in this contract.");

    asset quantity(config_get("orgminplnt.1"_n), seeds_symbol);

    check(sitr->balance >= quantity, 
        "The user does not have enough credit to create an organization" + sitr->balance.to_string() + " min: "+quantity.to_string());

    auto orgitr = organizations.find(orgaccount.value);
    check(orgitr == organizations.end(), "This organization already exists.");
    
    auto uitr = users.find(sponsor.value);
    check(uitr != users.end(), "Sponsor is not a Seeds account.");

    create_account(sponsor, orgaccount, orgfullname, publicKey);

    string memo =  "sow "+orgaccount.to_string();

    action(
        permission_level(_self, "active"_n),
        contracts::token,
        "transfer"_n,
        std::make_tuple(_self, contracts::harvest, quantity, memo)
    ).send();


    sponsors.modify(sitr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;           
    });

    organizations.emplace(_self, [&](auto & norg) {
        norg.org_name = orgaccount;
        norg.owner = sponsor;
        norg.planted = quantity;
        norg.status = status_regular;
    });

    addmember(orgaccount, sponsor, sponsor, ""_n);
    increase_size_by_one(get_self());
}

void organization::create_account(name sponsor, name orgaccount, string orgfullname, string publicKey) 
{
    action(
        permission_level{contracts::onboarding, "active"_n},
        contracts::onboarding, "onboardorg"_n,
        make_tuple(sponsor, orgaccount, orgfullname, publicKey)
    ).send();
}

ACTION organization::destroy(name organization, name owner) {
    check_owner(organization, owner);

    auto orgitr = organizations.find(organization.value);
    check(orgitr != organizations.end(), "organisation: the organization does not exist.");

    auto bitr = sponsors.find(owner.value);
    sponsors.modify(bitr, _self, [&](auto & mbalance) {
        mbalance.balance += orgitr -> planted;
    });

    members_tables members(get_self(), organization.value);
    auto mitr = members.begin();
    while(mitr != members.end()){
        mitr = members.erase(mitr);
    }
    
    auto org = organizations.find(organization.value);
    organizations.erase(org);

    decrease_size_by_one(get_self());

    // refund(owner, planted); this method could be called if we want to refund as soon as the user destroys an organization
}


ACTION organization::refund(name beneficiary, asset quantity) {
    require_auth(beneficiary);
    
    utils::check_asset(quantity);

    auto itr = sponsors.find(beneficiary.value);
    check(itr != sponsors.end(), "organisation: user has no entry in the balance table.");
    check(itr -> balance >= quantity, "organisation: user has not enough balance.");

    string memo = "refund";

    action(
        permission_level(_self, "active"_n),
        contracts::token,
        "transfer"_n,
        std::make_tuple(_self, beneficiary, quantity, memo)
    ).send();

    auto bitr = sponsors.find(_self.value);
    sponsors.modify(bitr, _self, [&](auto & mbalance) {
        mbalance.balance -= quantity;
    });

    sponsors.modify(itr, _self, [&](auto & mbalance) {
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

    auto orgitr = organizations.find(organization.value);

    organizations.modify(orgitr, _self, [&](auto & morg) {
        morg.owner = account;
    });
}

void organization::revert_previous_vote(name organization, name account) {
    vote_tables votes(get_self(), organization.value);

    auto vitr = votes.find(account.value);
    
    if(vitr != votes.end()){
        auto itr = organizations.find(organization.value);
        check(itr != organizations.end(), "organisation does not exist.");
        organizations.modify(itr, _self, [&](auto & morg) {
            morg.regen -= vitr -> regen_points;
        });
        
        auto avgitr = avgvotes.find(organization.value);
        check(avgitr != avgvotes.end(), "the organisation does not have an average entry");
        avgvotes.modify(avgitr, _self, [&](auto & avg){
            avg.total_sum -= vitr -> regen_points;
            avg.num_votes -= 1;
        });

        votes.erase(vitr);
    }
}

void organization::vote(name organization, name account, int64_t regen) {
    vote_tables votes(get_self(), organization.value);

    auto itr = organizations.find(organization.value);
    check(itr != organizations.end(), "organisation does not exist.");
    
    int64_t org_regen = itr -> regen + regen;

    organizations.modify(itr, _self, [&](auto & morg) {
        morg.regen = org_regen;
    });

    votes.emplace(_self, [&](auto & nvote) {
        nvote.account = account;
        nvote.timestamp = eosio::current_time_point().sec_since_epoch();
        nvote.regen_points = regen;
    });

    auto avgitr = avgvotes.find(organization.value);
    int64_t average;

    if (avgitr != avgvotes.end()) {
        int64_t total_sum = avgitr -> total_sum + regen;
        int64_t num_votes = (int64_t)(avgitr -> num_votes) + 1;
        average = total_sum / num_votes;

        avgvotes.modify(avgitr, _self, [&](auto & avg){
            avg.total_sum = total_sum;
            avg.num_votes = num_votes;
            avg.average = average;
        });
    } else {
        average = regen;
        avgvotes.emplace(_self, [&](auto & avg){
            avg.org_name = organization;
            avg.total_sum = regen;
            avg.num_votes = 1;
            avg.average = average;
        });
    }

    int64_t min_regen = (int64_t)config_get("orgratethrsh"_n);
    if (org_regen >= min_regen) {
        int64_t rate_cap = (int64_t)config.get(name("orgratecap").value, "The orgratecap parameter has not been initialized yet").value;
        org_regen = std::min(org_regen, rate_cap);
        auto itr_regen = regenscores.find(organization.value);
        int64_t score = org_regen * average;
        if (org_regen < 0 && score > 0) score *= -1; // negative scores stay negative
        if (itr_regen != regenscores.end()) {
            regenscores.modify(itr_regen, _self, [&](auto & rs){
                rs.regen_avg = score; // rename this to score - it's no longer an average, it's T*T/N 
            });
        } else {
            regenscores.emplace(_self, [&](auto & rs){
                rs.org_name = organization;
                rs.regen_avg = score;
                rs.rank = 0;
            });
            increase_size_by_one(regen_score_size);
        }
    }
}

ACTION organization::addregen(name organization, name account, uint64_t amount) {
    require_auth(account);
    check_user(account);

    uint64_t maxAmount = config.get(name("org.maxadd").value, "The parameter org.maxadd has not been initialized yet").value;
    amount = std::min(amount, maxAmount);

    revert_previous_vote(organization, account);
    vote(organization, account, amount * utils::get_rep_multiplier(account));
}


ACTION organization::subregen(name organization, name account, uint64_t amount) {
    require_auth(account);
    check_user(account);

    uint64_t minAmount = config.get(name("org.minsub").value, "The parameter org.minsub has not been initialized yet").value;
    amount = std::min(amount, minAmount);

    revert_previous_vote(organization, account);
    vote(organization, account, -1 * int64_t(amount) * utils::get_rep_multiplier(account));
}

ACTION organization::rankregens() {
    auto batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet");
    rankregen((uint64_t)0, (uint64_t)0, batch_size.value);
}

ACTION organization::rankregen(uint64_t start, uint64_t chunk, uint64_t chunksize) {
    require_auth(get_self());

    check(chunksize > 0, "chunk size must be > 0");

    uint64_t total = get_size(regen_score_size);
    if (total == 0) return;

    uint64_t current = chunk * chunksize;
    auto regen_score_by_avg_regen = regenscores.get_index<"byregenavg"_n>();
    auto rsitr = start == 0 ? regen_score_by_avg_regen.begin() : regen_score_by_avg_regen.lower_bound(start);
    uint64_t count = 0;

    while (rsitr != regen_score_by_avg_regen.end() && count < chunksize) {

        uint64_t rank = utils::rank(current, total);

        regen_score_by_avg_regen.modify(rsitr, _self, [&](auto & item) {
            item.rank = rank;
        });

        current++;
        count++;
        rsitr++;
    }

    if (rsitr != regen_score_by_avg_regen.end()) {
        action next_execution(
            permission_level("active"_n, get_self()),
            get_self(),
            "rankregen"_n,
            std::make_tuple((rsitr -> org_name).value, chunk + 1, chunksize)
        );
        transaction tx;
        tx.actions.emplace_back(next_execution);
        tx.delay_sec = 1;
        tx.send(regen_score_size.value, _self);
    }
}

void organization::check_referrals (name organization, uint64_t min_visitors_invited, uint64_t min_residents_invited) {
    auto refs_by_referrer = refs.get_index<"byreferrer"_n>();

    uint64_t residents = 0;
    uint64_t visitors = 0;

    auto ritr = refs_by_referrer.find(organization.value);

    while (ritr != refs_by_referrer.end() && ritr->referrer == organization && 
        (residents < min_residents_invited || visitors < min_visitors_invited)) {
        auto uitr = users.find(ritr->invited.value);
        if (uitr != users.end()) {
          if (uitr->status == "resident"_n || uitr->status == "citizen"_n) {
            residents++;
          }
          visitors++;
        }
        ritr++;
    }

    check(residents >= min_residents_invited, "organization has less than the required number of referred residents");
    check(visitors >= min_visitors_invited, "organization has less than the required number of referred visitors");
}

void organization::check_status_requirements (name organization, uint64_t status) {
    auto oitr = organizations.find(organization.value);
    check(oitr != organizations.end(), "organization not found");

    if (status == status_regular) { return; }

    uint64_t planted_min = get_config(name("rep.minplnt"));
    uint64_t regen_min_rank = get_config(name("rep.minrank"));
    uint64_t min_invited = get_config(name("rep.refrred"));
    uint64_t min_residents_invited = get_config(name("rep.resref"));
    check(oitr->status == status-1, 
        "organization is not " + (status_names[status-1]).to_string() + ", it can not become " + (status_names[status]).to_string());

    string status_str = std::to_string(status+1);

    uint64_t min_planted = config_get(name("orgminplnt." + status_str));
    uint64_t min_rep_rank = config_get(name("orgminrank." + status_str));
    uint64_t min_regen_score = config_get(name("org.rated." + status_str));
    uint64_t min_visitors_invited = config_get(name("org.visref." + status_str));
    uint64_t min_residents_invited = config_get(name("org.resref." + status_str));

    auto pitr = planted.get(organization.value, "organization does not have seeds planted");
    check(pitr.planted.amount >= min_planted, "organization has less than the required amount of seeds planted");

    rep_tables rep_t(contracts::accounts, name("org").value);
    auto repitr = rep_t.get(organization.value, "organization does not have reputation");
    check(repitr.rank >= min_rep_rank, "organization has less than the required reputation rank");

    check(oitr->regen >= min_regen_score, "organization has less than the required regen score");

    check_referrals(organization, min_visitors_invited, min_residents_invited);
}

void organization::update_status (name organization, uint64_t status) {
  auto oitr = organizations.find(organization.value);
  check(oitr != organizations.end(), "the organization does not exist");
  organizations.modify(oitr, _self, [&](auto& org) {
    org.status = status;
  });
}

void organization::history_update_org_status (name organization, uint64_t status) {
    action(
        permission_level(contracts::history, "active"_n),
        contracts::history,
        "updatestatus"_n,
        std::make_tuple(organization, status_names[status])
    ).send();
}

ACTION organization::makethrivble (name organization) {
    check_status_requirements(organization, status_thrivable);
    update_status(organization, status_thrivable);
    history_update_org_status(organization, status_thrivable);
}

ACTION organization::makeregen (name organization) {
    check_status_requirements(organization, status_regenerative);
    update_status(organization, status_regenerative);
    history_update_org_status(organization, status_regenerative);
}

ACTION organization::makesustnble (name organization) {
    check_status_requirements(organization, status_sustainable);
    update_status(organization, status_sustainable);
    history_update_org_status(organization, status_sustainable);
}

ACTION organization::makereptable(name organization) {
    check_status_requirements(organization, status_reputable);
    update_status(organization, status_reputable);
    history_update_org_status(organization, status_reputable);
}

ACTION organization::registerapp(name owner, name organization, name appname, string applongname) {
    require_auth(owner);
    check_owner(organization, owner);

    auto orgitr = organizations.find(organization.value);
    check(orgitr != organizations.end(), "This organization does not exist.");

    auto appitr = apps.find(appname.value);
    check(appitr == apps.end(), "This application already exists.");

    apps.emplace(_self, [&](auto & app){
        app.app_name = appname;
        app.org_name = organization;
        app.app_long_name = applongname;
        app.is_banned = false;
        app.number_of_uses = 0;
    });
}

ACTION organization::banapp(name appname) {
    require_auth(get_self());

    auto appitr = apps.find(appname.value);
    check(appitr != apps.end(), "This application does not exists.");
    
    apps.modify(appitr, _self, [&](auto & app){
        app.is_banned = true;
    });
}

ACTION organization::appuse(name appname, name account) {
    require_auth(account);
    check_user(account);

    auto appitr = apps.find(appname.value);
    check(appitr != apps.end(), "This application does not exists.");
    check(!(appitr -> is_banned), "Can not use a banned app.");
    
    dau_tables daus(get_self(), appname.value);

    uint64_t today_timestamp = get_beginning_of_day_in_seconds();
    
    auto dauitr = daus.find(account.value);
    if (dauitr != daus.end()) {
        if (dauitr -> date == today_timestamp) {
            daus.modify(dauitr, _self, [&](auto & dau){
                dau.number_app_uses += 1;
            });
        } else {
            if (dauitr -> number_app_uses != 0) {
                dau_history_tables dau_history(get_self(), appname.value); 
                dau_history.emplace(_self, [&](auto & dau_h){
                    dau_h.dau_history_id = dau_history.available_primary_key();
                    dau_h.account = dauitr -> account;
                    dau_h.date = dauitr -> date;
                    dau_h.number_app_uses = dauitr -> number_app_uses;
                });
            }
            daus.modify(dauitr, _self, [&](auto & dau){
                dau.date = today_timestamp;
                dau.number_app_uses = 1;
            }); 
        }
    } else {
        daus.emplace(_self, [&](auto & dau){
            dau.account = account;
            dau.date = today_timestamp;
            dau.number_app_uses = 1;
        });
    }

    apps.modify(appitr, _self, [&](auto & app){
        app.number_of_uses += 1;
    });
}

ACTION organization::cleandaus () {
    require_auth(get_self());

    uint64_t today_timestamp = get_beginning_of_day_in_seconds();

    auto appitr = apps.begin();
    while (appitr != apps.end()) {
        if (appitr -> is_banned) {
            appitr++;
            continue; 
        }
        action clean_dau_action(
            permission_level{get_self(), "active"_n},
            get_self(),
            "cleandau"_n,
            std::make_tuple(appitr -> app_name, today_timestamp, (uint64_t)0)
        );

        transaction tx;
        tx.actions.emplace_back(clean_dau_action);
        tx.delay_sec = 1;
        tx.send((appitr -> app_name).value + 1, _self);

        appitr++;
    }
}

ACTION organization::cleandau (name appname, uint64_t todaytimestamp, uint64_t start) {
    require_auth(get_self());

    auto appitr = apps.get(appname.value, "This application does not exist.");
    if (appitr.is_banned) { return; }

    auto batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet.").value;

    dau_tables daus(get_self(), appname.value);
    dau_history_tables dau_history(get_self(), appname.value); 

    auto dauitr = start == 0 ? daus.begin() : daus.lower_bound(start);
    uint64_t count = 0;

    while (dauitr != daus.end() && count < batch_size) {
        if (dauitr -> date != todaytimestamp) {
            dau_history.emplace(_self, [&](auto & dau_h){
                dau_h.dau_history_id = dau_history.available_primary_key();
                dau_h.account = dauitr -> account;
                dau_h.date = dauitr -> date;
                dau_h.number_app_uses = dauitr -> number_app_uses;
            });
            daus.modify(dauitr, _self, [&](auto & dau){
                dau.date = todaytimestamp;
                dau.number_app_uses = 0;
            });
        }
        count++;
        dauitr++;
    }

    if (dauitr != daus.end()) {
        action clean_dau_action(
            permission_level{get_self(), "active"_n},
            get_self(),
            "cleandau"_n,
            std::make_tuple(appname, todaytimestamp, (dauitr -> account).value)
        );

        transaction tx;
        tx.actions.emplace_back(clean_dau_action);
        tx.delay_sec = 1;
        tx.send((appitr.app_name).value + 1, _self);
    }
}

ACTION organization::scoretrxs() {
    scoreorgs(""_n);
}

ACTION organization::scoreorgs(name next) {
    require_auth(get_self());

    action(
        permission_level{contracts::harvest, "active"_n},
        contracts::harvest,
        "calctrxpts"_n,
        std::make_tuple()
    ).send();

    // auto itr = next == ""_n ? organizations.begin() : organizations.lower_bound(next.value);
    // if(itr != organizations.end()) {
    //     action(
    //         permission_level{contracts::history, "active"_n},
    //         contracts::history, "orgtxpoints"_n,
    //         std::make_tuple(itr -> org_name)
    //     ).send();
    //     itr++;
    //     if (itr != organizations.end()) {
    //         action next_execution(
    //             permission_level{get_self(), "active"_n},
    //             get_self(),
    //             "scoreorgs"_n,
    //             std::make_tuple(itr->org_name)
    //         );
    //         transaction tx;
    //         tx.actions.emplace_back(next_execution);
    //         tx.delay_sec = 1; // change this to 1 to test
    //         tx.send(next.value+2, _self);
    //     }
    // }
}

ACTION organization::testregensc (name organization, uint64_t score) {
    require_auth(get_self());

    auto oitr = organizations.find(organization.value);
    if (oitr != organizations.end()) {
        organizations.modify(oitr, _self, [&](auto & item){
            item.regen = score;
        });
    }
}

ACTION organization::teststatus (name organization, uint64_t status) {
    require_auth(get_self());
    update_status(organization, status);
    history_update_org_status(organization, status);
}
