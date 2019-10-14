#include <seeds.organization.hpp>

void organization::addorg(name organization, asset acctseed){
    require_auth(_self);
    print("add organization");
    auto it = orgtable.find(organization.value);
    check(it == orgtable.end(),"organization already exists");
    orgtable.emplace(_self,[&](auto &o){
        o.account = organization;
        o.per_user = acctseed;
        o.balance = asset(0,symbol(symbol_code("SEED"),4));

    });

}
void organization::editorg(name organization,asset acctseed){
    require_auth(_self);
    auto it = orgtable.find(organization.value);
    check(it != orgtable.end(),"organization does not exist");
    print("edit organization");
    orgtable.modify(it,_self,[&](auto &o){
        o.per_user = acctseed;
    });
}

void organization::giftuser(name organization,name user){
    require_auth(_self);
    auto it = orgtable.find(organization.value);
    check(it != orgtable.end(),"organization does not exist");
    check( it->balance < it->per_user,"organization does not have enough balance");
    depositfunds()
    

}

void organization::receivefund(name from,name to,asset quantity,string memo ){
    if (from == _self || from == "eosio.ram"_n || from == "eosio.stake"_n) {
        return;
    }

    if (to != _self) 
        return;

    if (memo == "setup") {
        // TODO: emit price data event
        return;
    }
    name organization = name(memo);
    auto it = orgtable.find(organization.value);
    check(it != orgtable.end(),"organization does not exist");
    print("organization exist");
        orgtable.modify(it,_self,[&](auto &o){
        o.balance += quantity;
    });
}

