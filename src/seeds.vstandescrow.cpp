#include <seeds.vstandescrow.hpp>

void vstandescrow::reset() {
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


void vstandescrow::init_balance(name user) {
    auto it = sponsors.find(user.value);
    
}

void vstandescrow::check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == seeds_symbol, "invalid asset");
}

void vstandescrow::trigger (const name&     trigger_source,
                            const name&     event_name,
                            const string&   notes) {
    require_auth (trigger_source);

    event_table e_t (get_self(), trigger_source.value);
    e_t.emplace (get_self(), [&](auto &e) {
        e.event_name    = event_name;
        e.notes         = notes;
    });
}

void vstandescrow::lock (   const name&         lock_type, 
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

    locks.emplace (get_self(), [&](auto &l) {
        l.id                = locks.available_primary_key();
        l.lock_type         = lock_type;
        l.sponsor           = sponsor;
        l.beneficiary       = beneficiary;
        l.quantity          = quantity;
        l.trigger_event     = trigger_event;
        l.trigger_source    = trigger_source;
        l.vesting_date      = vesting_date;
        l.notes             = notes;
    });
}

void vstandescrow::cancellock (const uint64_t& lock_id) {

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

void vstandescrow::ontransfer(name from, name to, asset quantity, string memo) {
   
    // only catch transfer to self of the SEEDS symbol (could be opened up, but would require other data structure changes)
    // get_first_receiver confirms that the tokens came from the right account
    if (get_first_receiver() == contracts::token  &&    // from SEEDS token account
        to  ==  get_self() &&                    // sent to escrow.seeds contract
        quantity.symbol == seeds_symbol) {       // SEEDS symbol

        
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
void vstandescrow::withdraw(name sponsor, asset quantity) {
    require_auth(sponsor);

    check_asset(quantity);  

    auto it = sponsors.find(sponsor.value);
    
    check(it != sponsors.end(), "vstandescrow: the user " + sponsor.to_string() + " does not have a balance entry");
    check(it -> liquid_balance >= quantity, "vstandescrow withdraw: the sponsor " + sponsor.to_string() + " does not have enough balance");

    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(_self, sponsor, quantity, "");

    // TODO: if, after this withdraw, sponsor's locked and liquid balances are zero, erase record
    auto it_s = sponsors.find(sponsor.value);
    sponsors.modify(it_s, _self, [&](auto & msponsor){
        msponsor.liquid_balance -= quantity;
    });
}

void vstandescrow::deduct_from_sponsor (name sponsor, asset locked_quantity) {
    auto it_c = sponsors.find(sponsor.value);

    // TODO: if it's the last of this sponsor's balance, erase record
    sponsors.modify(it_c, get_self(), [&](auto & msponsor){
        msponsor.locked_balance -= locked_quantity;
    });
}

void vstandescrow::claim(name beneficiary) {
    require_auth(beneficiary);

    auto locks_by_beneficiary = locks.get_index<"bybneficiary"_n>();
    auto it = locks_by_beneficiary.find(beneficiary.value);

    asset total_quantity = asset(0, seeds_symbol);
    check(it != locks_by_beneficiary.end(), "vstandscrow: The user " + beneficiary.to_string() + " does not have any locks.");

    while(it != locks_by_beneficiary.end()){

        if(it -> beneficiary != beneficiary)
            break;

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
                total_quantity += it -> quantity;
                it = locks_by_beneficiary.erase(it);
            } else {
                it++;
            }
        } else {
            it++;
        }
    }

    check(total_quantity > asset(0, seeds_symbol), "vstandscrow: The beneficiary does not have any available locks, try to claim them after their vesting date or triggering event");

    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {get_self(), "active"_n}};
    action.send(get_self(), beneficiary, total_quantity, "");
}











