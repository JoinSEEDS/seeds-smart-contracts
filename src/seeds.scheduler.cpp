#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <string>


bool scheduler::isRdyToExec(name operation){
    auto itr = operations.find(operation.value);
    check(itr != operations.end(), "The operation does not exist.");
    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();
    uint64_t periods = 0;

    uint64_t seconds_expired = (timestamp - itr -> timestamp);

    periods = seconds_expired / itr -> period;

    if(periods > 0) return true;
    return false;
}


ACTION scheduler::reset() {
    require_auth(_self);

    auto itr = operations.begin();
    while(itr != operations.end()){
        itr = operations.erase(itr);
    }
}


ACTION scheduler::configop(name action, name contract, uint64_t period_sec) {
    require_auth(_self);

    auto itr = operations.find(action.value);

    if(itr != operations.end()){
        operations.modify(itr, _self, [&](auto & moperation) {
            moperation.operation = action;
            moperation.contract = contract;
            moperation.period = period_sec;
        });
    }
    else{
        operations.emplace(_self, [&](auto & noperation) {
            noperation.operation = action;
            noperation.contract = contract;
            noperation.period = period_sec;
            noperation.timestamp = current_time_point().sec_since_epoch();
        });
    }

}


ACTION scheduler::confirm(name operation) {
    require_auth(_self);

    auto itr = operations.find(operation.value);
    check(itr != operations.end(), "Operation does not exist");

    operations.modify(itr, _self, [&](auto & moperation) {
        moperation.timestamp = current_time_point().sec_since_epoch();
    });
}


ACTION scheduler::execute() {
   require_auth(_self);

    /*
        Just as quick reminder.
        
        1.- To run inline actions from an external contract eosio.code permission is needed, the way to add it is:
                
                cleos set account permission YOUR_ACCOUNT active 
                '{"threshold": 1,"keys": [{"key": "YOUR_PUBLIC_KEY","weight": 1}], 
                "accounts": [{"permission":{"actor":"YOUR_ACCOUNT","permission":"eosio.code"},"weight":1}]}' 
                -p YOUR_ACCOUNT@owner


        2.- This function does not require authorization from _self, to make this function more secure, 
            a custom permission for running this function is needed,
            the way we can assing a custom permission to an account is:
                
                cleos set account permission YOUR_ACCOUNT CUSTOM_PERMISSION 
                '{"threshold":1,"keys":[{"key":"YOUR_PUBLIC_KEY","weight":1}]}' "active" -p YOUR_ACCOUNT@active

            Then we need to link the permission to an action, this is the way:

                cleos set action permission YOUR_ACCOUNT CONTRACT ACTION CUSTOM_PERMISSION
            
            By doing this, we are restricting the ACTION to be callable only for an account who has the CUSTOM_PERMISSION
    */

    auto itr = operations.begin();
    while(itr != operations.end()) {
        if(isRdyToExec(itr -> operation)){
            
            action a = action(
                //permission_level{contracts::forum, "period"_n},
                //permission_level(get_self(), "scheduled"_n),
                permission_level{get_self(), "active"_n},
                itr -> contract,
                itr -> operation,
                std::make_tuple()
            );

            a.send();

            //transaction tx;
            //tx.actions.emplace_back(a);
            //tx.send(eosio::current_time_point().sec_since_epoch() + 10, _self, false);

            action c = action(
                permission_level{get_self(), "active"_n},
                get_self(),
                "confirm"_n,
                std::make_tuple(itr -> operation)
            );

            c.send();

            break;
        }
        itr++;
    }

    transaction trx{};
    trx.actions.emplace_back(
        permission_level(_self, "active"_n),
        _self,"execute"_n,
        std::make_tuple()
    );
    trx.delay_sec = 60;
    trx.send(eosio::current_time_point().sec_since_epoch() + 30, _self);

    
}



EOSIO_DISPATCH(scheduler,(configop)(execute)(reset)(confirm));
