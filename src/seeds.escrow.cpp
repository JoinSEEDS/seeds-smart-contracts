#include <seeds.escrow.hpp>

void escrow::reset() {
    require_auth(get_self());

    auto it_e = locks.begin();
    while(it_e != locks.end()){
        it_e = locks.erase(it_e);
    }

    auto it_s = sponsors.begin();
    while(it_s != sponsors.end()) {
        it_s = sponsors.erase(it_s);
    }
}

void escrow::check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == utils::seeds_symbol, "invalid asset");
}

void escrow::trigger (const name&     trigger_source,
                            const name&     event_name,
                            const string&   notes) {
    require_auth (trigger_source);

    event_table e_t (get_self(), trigger_source.value);
    e_t.emplace (get_self(), [&](auto &e) {
        e.event_name    = event_name;
        e.notes         = notes;
    });

    if (trigger_source == trigger_source_hypha_dao && event_name == trigger_event_golive) {
        sendtopool(uint64_t(0), config_get("batchsize"_n));
    }
}

void escrow::sendtopool (uint64_t start, uint64_t chunksize) {
    require_auth(get_self());

    auto litr = start == 0 ? locks.begin() : locks.lower_bound(start);
    uint64_t current = 0;
    
    while (litr != locks.end() && current < chunksize) {
        if (litr->trigger_source == trigger_source_hypha_dao && litr->trigger_event == trigger_event_golive) {
            action a(
                permission_level(get_self(), "active"_n),
                get_self(),
                "miglock"_n,
                std::make_tuple(litr->id)
            );
            transaction tx;
            tx.actions.emplace_back(a);
            tx.delay_sec = 1;
            tx.send(litr->id, _self);
        }
        litr++;
        current += 3;
    }

    if (litr != locks.end()) {
        action next_execution(
            permission_level(get_self(), "active"_n),
            get_self(),
            "sendtopool"_n,
            std::make_tuple(litr->id, chunksize)
        );

        transaction tx;
        tx.actions.emplace_back(next_execution);
        tx.delay_sec = 1;
        tx.send(litr->id, _self);
    }
}

void escrow::miglock (uint64_t lock_id) {
    require_auth(get_self());

    auto litr = locks.find(lock_id);
    check(litr != locks.end(), "lock not found");

    if (litr->trigger_source == trigger_source_hypha_dao && litr->trigger_event == trigger_event_golive) {

        if (!litr->notes.empty()) {
            std::size_t found = litr->notes.find(string("proposal id: "));
            if (found != std::string::npos) {
                string prop_id_string = litr->notes.substr(13, string::npos);
                uint64_t prop_id = uint64_t(std::stoi(prop_id_string));
                action(
                    permission_level(contracts::proposals, "active"_n),
                    contracts::proposals,
                    "checkprop"_n,
                    std::make_tuple(prop_id, string("proposal is not passing, lock can not be claimed"))
                ).send();
                action(
                    permission_level(contracts::proposals, "active"_n),
                    contracts::proposals,
                    "doneprop"_n,
                    std::make_tuple(prop_id)
                ).send();
            }
        }

        deduct_from_sponsor(litr->sponsor, litr->quantity);
        send_transfer(contracts::pool, litr->quantity, litr->beneficiary.to_string());
        litr = locks.erase(litr);
    }
}

void escrow::lock (   const name&         lock_type, 
                            const name&         sponsor, 
                            const name&         beneficiary,
                            const asset&        quantity, 
                            const name&         trigger_event, 
                            const name&         trigger_source,
                            const time_point&   vesting_date,
                            const string&       notes) {
    require_auth (sponsor);

    // only allow SEEDS
    check_asset (quantity);

    // check if the sponsor has enough balance
    auto it_b = sponsors.find(sponsor.value);        
    check(it_b != sponsors.end(), "vstandscrow: The user " + sponsor.to_string() + " does not have a balance entry");
    check(it_b -> liquid_balance >= quantity, "vstandscrow lock: The sponsor " + sponsor.to_string() + " does not have enough balance");
    sponsors.modify(it_b, _self, [&](auto & msponsor){
        msponsor.liquid_balance -= quantity;
        msponsor.locked_balance += quantity;
    });

    uint64_t lock_id = locks.available_primary_key();

    locks.emplace (get_self(), [&](auto &l) {
        l.id                = lock_id;
        l.lock_type         = lock_type;
        l.sponsor           = sponsor;
        l.beneficiary       = beneficiary;
        l.quantity          = quantity;
        l.trigger_event     = trigger_event;
        l.trigger_source    = trigger_source;
        l.vesting_date      = vesting_date;
        l.notes             = notes;
    });
    if (!notes.empty()) {
        std::size_t found = notes.find(string("proposal id: "));
        if (found != std::string::npos) {
            string prop_id_string = notes.substr(13, string::npos);
            uint64_t prop_id = uint64_t(std::stoi(prop_id_string));
            action(
                permission_level(contracts::proposals, "active"_n),
                contracts::proposals,
                "addcampaign"_n,
                std::make_tuple(prop_id, lock_id)
            ).send();
        }
    }
}

void escrow::cancellock (const uint64_t& lock_id) {

    auto l_itr = locks.find (lock_id);
    check (l_itr != locks.end(), "Lock ID " + std::to_string(lock_id) + " does not exist.");

    require_auth (l_itr->sponsor);

    auto it_f = sponsors.find(l_itr->sponsor.value);
    sponsors.modify(it_f, get_self(), [&](auto & msponsor){
        msponsor.liquid_balance += l_itr->quantity;
        msponsor.locked_balance -= l_itr->quantity;
    });

    locks.erase (l_itr);
}

void escrow::ontransfer(name from, name to, asset quantity, string memo) {
   
    // only catch transfer to self of the SEEDS symbol (could be opened up, but would require other data structure changes)
    // get_first_receiver confirms that the tokens came from the right account
    if (get_first_receiver() == contracts::token  &&    // from SEEDS token account
        to  ==  get_self() &&                    // sent to escrow.seeds contract
        quantity.symbol == utils::seeds_symbol) {       // SEEDS symbol

        
        auto s_itr = sponsors.find (from.value);
        if (s_itr == sponsors.end()) {
            sponsors.emplace(get_self(), [&](auto &s){
                s.sponsor = from;
                s.liquid_balance = quantity;
            });
        } else {
            sponsors.modify (s_itr, get_self(), [&](auto &s) {
                s.liquid_balance += quantity;
            });
        }
    }   
}

// TODO: should not allow withdraw with funds that have associated locks
void escrow::withdraw(name sponsor, asset quantity) {
    require_auth(sponsor);

    check_asset(quantity);  

    auto it = sponsors.find(sponsor.value);
    
    check(it != sponsors.end(), "escrow: the user " + sponsor.to_string() + " does not have a balance entry");
    check(it -> liquid_balance >= quantity, "escrow withdraw: the sponsor " + sponsor.to_string() + " does not have enough balance");

    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(_self, sponsor, quantity, "");

    // TODO: if, after this withdraw, sponsor's locked and liquid balances are zero, erase record
    auto it_s = sponsors.find(sponsor.value);
    sponsors.modify(it_s, _self, [&](auto & msponsor){
        msponsor.liquid_balance -= quantity;
    });
}

void escrow::deduct_from_sponsor (name sponsor, asset locked_quantity) {
    auto it_c = sponsors.find(sponsor.value);

    // TODO: if it's the last of this sponsor's balance, erase record
    sponsors.modify(it_c, get_self(), [&](auto & msponsor){
        msponsor.locked_balance -= locked_quantity;
    });
}

void escrow::claim(name beneficiary) {
    require_auth(beneficiary);

    auto locks_by_beneficiary = locks.get_index<"bybneficiary"_n>();
    auto it = locks_by_beneficiary.find(beneficiary.value);

    asset zero = asset(0, utils::seeds_symbol);
    asset total_quantity = zero;
    asset pool_quantity = zero;
    check(it != locks_by_beneficiary.end(), "vstandscrow: The user " + beneficiary.to_string() + " does not have any locks.");

    while(it != locks_by_beneficiary.end()){

        if(it -> beneficiary != beneficiary)
            break;

        if (!it->notes.empty()) {
            std::size_t found = it->notes.find(string("proposal id: "));
            if (found != std::string::npos) {
                string prop_id_string = it->notes.substr(13, string::npos);
                uint64_t prop_id = uint64_t(std::stoi(prop_id_string));
                action(
                    permission_level(contracts::proposals, "active"_n),
                    contracts::proposals,
                    "checkprop"_n,
                    std::make_tuple(prop_id, string("proposal is not passing, lock can not be claimed"))
                ).send();
                action(
                    permission_level(contracts::proposals, "active"_n),
                    contracts::proposals,
                    "doneprop"_n,
                    std::make_tuple(prop_id)
                ).send();
            }
        }

        if (it->lock_type == "time"_n) {
            if(it -> vesting_date <= current_time_point()){
                deduct_from_sponsor (it->sponsor, it->quantity);
                total_quantity += it -> quantity;
                it = locks_by_beneficiary.erase(it);
            } else {
                it++;
            }
        } else if (it->lock_type == "event"_n) {
            event_table e_t (get_self(), it->trigger_source.value);
            auto e_itr = e_t.find (it->trigger_event.value);
            if (e_itr != e_t.end() && e_itr->event_date <= current_time_point()) {
                deduct_from_sponsor (it->sponsor, it->quantity);
                if (it->trigger_source == trigger_source_hypha_dao && it->trigger_event == trigger_event_golive) {
                    pool_quantity += it->quantity;
                } else {
                    total_quantity += it->quantity;
                }
                it = locks_by_beneficiary.erase(it);
            } else {
                it++;
            }
        } else {
            it++;
        }
    }

    check(total_quantity > zero || pool_quantity > zero, 
        "vstandscrow: The beneficiary does not have any available locks, try to claim them after their vesting date or triggering event");

    if (total_quantity > zero) {
        send_transfer(beneficiary, total_quantity, string(""));
    }
    if (pool_quantity > zero) {
        send_transfer(contracts::pool, pool_quantity, beneficiary.to_string());
    }
}


void escrow::resettrigger (const name & trigger_source) {
    require_auth(get_self());

    event_table e_t (get_self(), trigger_source.value);
    auto eitr = e_t.begin();
    while (eitr != e_t.end()) {
        eitr = e_t.erase(eitr);
    }
}

void escrow::triggertest (const name&     trigger_source,
                            const name&     event_name,
                            const string&   notes) {
    require_auth (get_self());

    event_table e_t (get_self(), trigger_source.value);
    e_t.emplace (get_self(), [&](auto &e) {
        e.event_name    = event_name;
        e.notes         = notes;
    });

    if (trigger_source == trigger_source_hypha_dao && event_name == trigger_event_golive) {
        sendtopool(uint64_t(0), config_get("batchsize"_n));
    }
}

void escrow::send_transfer (const name & beneficiary, const asset & quantity, const string & memo) {
    token::transfer_action action {name(contracts::token), {get_self(), "active"_n}};
    action.send(get_self(), beneficiary, quantity, memo);
}


