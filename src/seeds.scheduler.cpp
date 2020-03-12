#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <string>


bool scheduler::isRdyToExec(name operation){
    auto itr = operations.find(operation.value);

    if (itr == operations.end()) {
        return false;
    }
    if(itr -> pause > 0) {
        print("transaction " + operation.to_string() + " is paused");
        return false;
    }

    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();
    uint64_t periods = 0;

    periods = (timestamp - itr -> timestamp) / itr -> period;

    print("\nPERIODS: " + std::to_string(periods) + ", timestamp: " + std::to_string(timestamp) + ", timestap: " + std::to_string(itr->timestamp) );

    if(periods > 0) return true;
    return false;
}


ACTION scheduler::reset() {
    require_auth(_self);

    auto itr = operations.begin();
    while(itr != operations.end()){
        itr = operations.erase(itr);
    }

    std::vector<name> id_v = { 
        name("rep.harvest"), 
        name("trx.harvest"), 
        name("plad.harvest") 
    };
    
    std::vector<name> operations_v = {
        name("calcrep"),
        name("calctrx"),
        name("calcplanted")
    };

    std::vector<name> contracts_v = {
        contracts::harvest,
        contracts::harvest,
        contracts::harvest
    };

    std::vector<uint64_t> delay_v = {
        60,
        60,
        60
    };

    int i = 0;
    while(i < 3){
        operations.emplace(_self, [&](auto & noperation){
            noperation.id = id_v[i];
            noperation.operation = operations_v[i];
            noperation.contract = contracts_v[i];
            noperation.pause = 0;
            noperation.period = delay_v[i];
            noperation.timestamp = current_time_point().sec_since_epoch();
        });
        i++;
    }
}


ACTION scheduler::configop(name id, name action, name contract, uint64_t period) {
    require_auth(_self);

    auto itr = operations.find(id.value);
    
    if(itr != operations.end()){
        operations.modify(itr, _self, [&](auto & moperation) {
            moperation.operation = action;
            moperation.contract = contract;
            moperation.period = period;
            moperation.pause = 0;
        });
    }
    else{
        operations.emplace(_self, [&](auto & noperation) {
            noperation.id = id;
            noperation.pause = 0;
            noperation.operation = action;
            noperation.contract = contract;
            noperation.period = period_sec;
            noperation.timestamp = current_time_point().sec_since_epoch();
        });
    }
}

ACTION scheduler::removeop(name id) {
    require_auth(get_self());

    auto itr = operations.find(id.value);
    check(itr != operations.end(), contracts::scheduler.to_string() + ": the operation " + id.to_string() + " does not exist");

    operations.erase(itr);
}

ACTION scheduler::pauseop(name id, uint8_t pause) {
    require_auth(get_self());

    auto itr = operations.find(id.value);
    check(itr != operations.end(), contracts::scheduler.to_string() + ": the operation " + id.to_string() + " does not exist");

    operations.modify(itr, _self, [&](auto & moperation) {
        moperation.pause = pause;
    });
}

ACTION scheduler::confirm(name operation) {
    require_auth(_self);

    print("Confirm the execution of " + operation.to_string());

    auto itr = operations.find(operation.value);
    check(itr != operations.end(), "Operation does not exist");

    operations.modify(itr, _self, [&](auto & moperation) {
        moperation.timestamp = current_time_point().sec_since_epoch();
    });
}


ACTION scheduler::execute() {
   require_auth(_self);

   print("Executing...");

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

    auto it_s = config.find(seconds_to_execute.value);
    check(it_s != config.end(), contracts::scheduler.to_string() + ": the parameter " + seconds_to_execute.to_string() + " is not configured in " + contracts::settings.to_string());

    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "execute"_n,
        std::make_tuple()
    );

    print("Creating the diferred transaction...");

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = it_s -> value;
    tx.send(contracts::scheduler.value /*eosio::current_time_point().sec_since_epoch() + 30*/, _self);
    
    print("transaction sended");

    while(itr != operations.end()) {
        if(isRdyToExec(itr -> id)){

            print("Operation to be executed: " + itr -> id.to_string());

            action a = action(
                permission_level{itr -> contract, "execute"_n},
                itr -> contract,
                itr -> operation,
                std::make_tuple()
            );

            transaction txa;
            txa.actions.emplace_back(a);
            txa.delay_sec = 0;
            txa.send(eosio::current_time_point().sec_since_epoch() + 20, _self);

            // a.send();

            action c = action(
                permission_level{get_self(), "active"_n},
                get_self(),
                "confirm"_n,
                std::make_tuple(itr -> id)
            );

            c.send();

            break;
        }
        itr++;
    }

}

ACTION scheduler::cancelexec() {
    require_auth(get_self());
    cancel_deferred(contracts::scheduler.value);
}


EOSIO_DISPATCH(scheduler,(configop)(execute)(reset)(confirm)(pauseop)(removeop)(cancelexec));
