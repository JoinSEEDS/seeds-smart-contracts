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


ACTION scheduler::execute() {
    require_auth(_self);

    auto itr = operations.begin();
    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();
    uint64_t periods = 0;

    while(itr != operations.end()) {
        periods = ((timestamp - itr -> timestamp) * 10000) / itr -> period;

        // check(1 > 2, "Periods: " + std::to_string(periods) + ", t = " + std::to_string(timestamp) + ", timestamp  = " + std::to_string(itr -> timestamp) + ", period = " + std::to_string(itr -> period) + ", t-t = " + std::to_string(timestamp-(itr->timestamp)));
        
        if(periods > 0){
            
            action a = action(
                permission_level{"forum"_n, "active"_n},
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



EOSIO_DISPATCH(scheduler,(configop)(execute));

