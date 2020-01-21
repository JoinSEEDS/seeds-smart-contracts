#include <seeds.vestingescrow.hpp>

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


void vstandescrow::create(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date) {
    require_auth(sponsor);

    TypeCreate type = createNormal;
    createaux(sponsor, beneficiary, quantity, vesting_date, type);
}


void vstandescrow::createescrow(name sponsor_bucket, name beneficiary_org, asset quantity, uint64_t vesting_date) {
    require_auth(sponsor_bucket);

    // here we perform the transfer
    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(sponsor_bucket, beneficiary_org, quantity, "");

    TypeCreate type = createEscrow;
    createaux(sponsor_bucket, beneficiary_org, quantity, vesting_date, type);
}


void vstandescrow::cancel(name sponsor, name beneficiary) {
    
    // how does this method will require multi-signing?

    require_auth(sponsor);
    auto escrows_by_sponsor = escrows.get_index<"bysponsor"_n>();
    auto it = escrows_by_sponsor.find(sponsor.value);
    TypeCreate type = createNormal;

    check(it != escrows_by_sponsor.end(), "vstandscrow: The user " + sponsor.to_string() + " does not have a escrow entry");

    while(it != escrows_by_sponsor.end()){
        if(it -> beneficiary == beneficiary && it -> type == type){

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


void vstandescrow::cancelescrow(name sponsor, name beneficiary) {
    require_auth(sponsor);

    auto escrows_by_sponsor = escrows.get_index<"bysponsor"_n>();
    auto it = escrows_by_sponsor.find(sponsor.value);
    TypeCreate type = createEscrow;

    while(it != escrows_by_sponsor.end()){
        if(it -> beneficiary == beneficiary && it -> type == type){

            auto token_account = contracts::token;
            token::transfer_action action{name(token_account), {_self, "active"_n}};
            action.send(_self, sponsor, it -> quantity, "");

            escrows_by_sponsor.erase(it);
            break;
        }
        it++;
    }

}


void vstandescrow::claim(name beneficiary) {
    require_auth(beneficiary);

    // what would happen if the beneficiary has more than one escrow entry?

    auto escrows_by_beneficiary = escrows.get_index<"bybneficiary"_n>();
    auto it = escrows_by_beneficiary.find(beneficiary.value);

    uint64_t current_time = eosio::current_time_point().sec_since_epoch();

    check(it != escrows_by_beneficiary.end(), "vstandscrow: The user " + beneficiary.to_string() + " does not have and escrow entry");
    check(it -> vesting_date <= current_time, "vstandscrow: The beneficiary can not claim the funds before the vesting date");

    // transfer the tokens to the beneficiary
    auto token_account = contracts::token;
    token::transfer_action action{name(token_account), {_self, "active"_n}};
    action.send(_self, beneficiary, it -> quantity, "");

    // subtract the quantity to the contract
    auto it_c = sponsors.find(get_self().value);
    sponsors.modify(it_c, _self, [&](auto & msponsor){
        msponsor.balance -= it -> quantity;
    });

    // delete the escrow entry
    escrows_by_beneficiary.erase(it);
}











