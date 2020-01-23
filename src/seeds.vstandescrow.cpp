#include <seeds.vstandescrow.hpp>

void vstandescrow::reset() {
    require_auth(get_self());

    auto it_e = escrows.begin();
    while(it_e != escrows.end()){
        it_e = escrows.erase(it_e);
    }

    auto it_s = sponsors.begin();
    while(it_s != sponsors.end()) {
        it_s = sponsors.erase(it_s);
    }
}


void vstandescrow::init_balance(name user) {
    auto it = sponsors.find(user.value);
    if(it == sponsors.end()) {
        sponsors.emplace(_self, [&](auto & nsponsor){
            nsponsor.sponsor = user;
            nsponsor.balance = asset(0, seeds_symbol);
        });
    }
}


void vstandescrow::check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == seeds_symbol, "invalid asset");
}


void vstandescrow::createaux(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date, uint8_t type){
    // check if the sponsor has enough balance
    auto it_b = sponsors.find(sponsor.value);
    
    check(it_b != sponsors.end(), "vstandscrow: The user " + sponsor.to_string() + " does not have a balance entry");
    check(it_b -> balance >= quantity, "vstandscrow: The sponsor " + sponsor.to_string() + " does not have enough balance");


    // check for the beneficiary constraints (pending)

    sponsors.modify(it_b, _self, [&](auto & msponsor){
        msponsor.balance -= quantity;
    });

    escrows.emplace(_self, [&](auto & nescrow){
        nescrow.id = escrows.available_primary_key();
        nescrow.sponsor = sponsor;
        nescrow.beneficiary = beneficiary;
        nescrow.quantity = quantity;
        nescrow.vesting_date = vesting_date;
        nescrow.type = type;
    });
}


void vstandescrow::cancelaux(name sponsor, name beneficiary) {
    auto escrows_by_sponsor = escrows.get_index<"bysponsor"_n>();
    auto it = escrows_by_sponsor.find(sponsor.value);
    TypeCreate type = createNormal;

    check(it != escrows_by_sponsor.end(), "vstandscrow: The user " + sponsor.to_string() + " does not have a escrow entry");

    while(it != escrows_by_sponsor.end()){
        if(it -> beneficiary == beneficiary){

            auto it_s = sponsors.find(sponsor.value);
            sponsors.modify(it_s, _self, [&](auto & msponsor){
                msponsor.balance += it -> quantity;
            });

            escrows_by_sponsor.erase(it);
            break;
        }
        it++;
    }
}


void vstandescrow::ontransfer(name from, name to, asset quantity, string memo) {
    if(to == get_self()) {
        init_balance(from);
        init_balance(to);

        auto it_f = sponsors.find(from.value);
        sponsors.modify(it_f, _self, [&](auto & msponsor){
            msponsor.balance += quantity;
        });

        auto it_t = sponsors.find(to.value);
        sponsors.modify(it_t, _self, [&](auto & msponsor){
            msponsor.balance += quantity;
        });
    }
}


void vstandescrow::withdraw(name sponsor, asset quantity) {
    require_auth(sponsor);

    check_asset(quantity);

    auto it = sponsors.find(sponsor.value);
    
    check(it != sponsors.end(), "vstandescrow: the user " + sponsor.to_string() + " does not have a balance entry");
    check(it -> balance >= quantity, "vstandescrow: the sponsor " + sponsor.to_string() + " does not have enough balance");

    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(_self, sponsor, quantity, "");

    // subtract the quantity to the contract
    auto it_c = sponsors.find(get_self().value);
    sponsors.modify(it_c, _self, [&](auto & msponsor){
        msponsor.balance -= quantity;
    });

    auto it_s = sponsors.find(sponsor.value);
    sponsors.modify(it_s, _self, [&](auto & msponsor){
        msponsor.balance -= quantity;
    });
}


void vstandescrow::create(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date) {
    require_auth(sponsor);

    TypeCreate type = createNormal;
    createaux(sponsor, beneficiary, quantity, vesting_date, type);
}


void vstandescrow::createescrow(name sponsor_bucket, name beneficiary_org, asset quantity, uint64_t vesting_date) {
    require_auth(sponsor_bucket);

    TypeCreate type = createEscrow;
    createaux(sponsor_bucket, beneficiary_org, quantity, vesting_date, type);
}


void vstandescrow::cancel(name sponsor, name beneficiary) {
    require_auth(sponsor);
    cancelaux(sponsor, beneficiary);
}


void vstandescrow::cancelescrow(name sponsor, name beneficiary) {
    require_auth(sponsor);
    cancelaux(sponsor, beneficiary);
}


void vstandescrow::claim(name beneficiary) {
    require_auth(beneficiary);

    auto escrows_by_beneficiary = escrows.get_index<"bybneficiary"_n>();
    auto it = escrows_by_beneficiary.find(beneficiary.value);

    uint64_t current_time = eosio::current_time_point().sec_since_epoch();

    asset total_quantity = asset(0, seeds_symbol);

    check(it != escrows_by_beneficiary.end(), "vstandscrow: The user " + beneficiary.to_string() + " does not have and escrow entry");

    while(it != escrows_by_beneficiary.end()){
        if(it -> vesting_date <= current_time && it -> beneficiary == beneficiary){
            total_quantity += it -> quantity;
            it = escrows_by_beneficiary.erase(it);
        }
        else{
            break;
        }
    }

    check(total_quantity > asset(0, seeds_symbol), "vstandscrow: The beneficiary does not have any available escrows, try to claim them after their vesting date");

    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(_self, beneficiary, total_quantity, "");

    auto it_c = sponsors.find(get_self().value);
    sponsors.modify(it_c, _self, [&](auto & msponsor){
        msponsor.balance -= total_quantity;
    });

}











