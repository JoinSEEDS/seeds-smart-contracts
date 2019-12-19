#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <string>



ACTION scheduler::reset() {
    require_auth(_self);

    auto itr = operations.begin();
    while(itr != operations.end()){
        itr = operations.erase(itr);
    }
}


ACTION scheduler::configop(name action, name contract, uint64_t period) {
    require_auth(_self);

    auto itr = operations.find(action.value);

    if(itr != operations.end()){
        operations.modify(itr, _self, [&](auto & moperation) {
            moperation.operation = action;
            moperation.contract = contract;
            moperation.period = period;
        });
    }
    else{
        operations.emplace(_self, [&](auto & noperation) {
            noperation.operation = action;
            noperation.contract = contract;
            noperation.period = period;
            noperation.timestamp = current_time_point().sec_since_epoch();
        });
    }

}


ACTION scheduler::noop(){
    //require_auth(_self);
    print("HOLAAAA!");
    return;
}


ACTION scheduler::execute() {
    // require_auth(_self);

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
    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();
    uint64_t periods = 0;

    while(itr != operations.end()) {
        periods = ((timestamp - itr -> timestamp) * 10000) / itr -> period;

        print("Mira: " + std::to_string(periods));
        if(periods > 0){
            
            action a = action(
                permission_level{get_self(), "active"_n},
                itr -> contract,
                itr -> operation,
                std::make_tuple()
            );

            a.send();

            operations.modify(itr, _self, [&](auto & moperation) {
                moperation.timestamp = current_time_point().sec_since_epoch();
            });

            break;
        }
        itr++;
    }
}



EOSIO_DISPATCH(scheduler,(configop)(execute)(noop));
