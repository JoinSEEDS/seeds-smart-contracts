#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <string>


bool scheduler::is_ready_to_execute(name operation){
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

    print("\nPERIODS: " + std::to_string(periods) + ", current_time: " + std::to_string(timestamp) + ", last_timestap: " + std::to_string(itr->timestamp) );

    if(periods > 0) return true;
    return false;
}


ACTION scheduler::start() {
    require_auth(get_self());
    execute();
}

ACTION scheduler::stop() {
    require_auth(get_self());
    cancel_exec();
}

ACTION scheduler::reset() {
    require_auth(get_self());

    cancel_exec();

    auto itr = operations.begin();
    while(itr != operations.end()){
        itr = operations.erase(itr);
    }

    auto titr = test.begin();
    while(titr != test.end()){
        titr = test.erase(titr);
    }

    std::vector<name> id_v = { 
        name("exch.period"),
        name("tokn.resetw"),

        name("acct.rankrep"),
        name("acct.rankcbs"),

        name("hrvst.ranktx"),
        name("hrvst.rankpl"),

        name("hrvst.calccs"), // after the above 4
        name("hrvst.rankcs"), 
        name("hrvst.calctx"), // 24h

        name("org.clndaus"),
        name("org.clcregen"),
        name("org.rankcbs"),
        name("org.clctrpts"),
    };
    
    std::vector<name> operations_v = {
        name("onperiod"),
        name("resetweekly"),

        name("rankreps"),
        name("rankcbss"),
        
        name("ranktxs"),
        name("rankplanteds"),

        name("calccss"),
        name("rankcss"),
        name("calctrxpts"),

        name("cleandaus"),
        name("rankregens"),
        name("rankcbsorgs"),
        name("calctrxpts"),
    };

    std::vector<name> contracts_v = {
        contracts::exchange,
        contracts::token,

        contracts::accounts,
        contracts::accounts,

        contracts::harvest,
        contracts::harvest,

        contracts::harvest,
        contracts::harvest,
        contracts::harvest,

        contracts::organization,
        contracts::organization,
        contracts::organization,
        contracts::organization,
    };

    std::vector<uint64_t> delay_v = {
        utils::seconds_per_day * 7,
        utils::seconds_per_day * 7,

        utils::seconds_per_hour,
        utils::seconds_per_hour,

        utils::seconds_per_hour,
        utils::seconds_per_hour,

        utils::seconds_per_hour,
        utils::seconds_per_hour,
        utils::seconds_per_day,

        utils::seconds_per_day / 2,
        utils::seconds_per_day,
        utils::seconds_per_day,
        utils::seconds_per_day / 3,
    };

    uint64_t now = current_time_point().sec_since_epoch();

    std::vector<uint64_t> timestamp_v = {
        now,
        now,

        now - utils::seconds_per_hour, 
        now - utils::seconds_per_hour, 

        now - utils::seconds_per_hour, 
        now - utils::seconds_per_hour, 

        now + 300 - utils::seconds_per_hour, // kicks off 5 minutes later
        now + 600 - utils::seconds_per_hour, // kicks off 10 minutes later
        now,

        now,
        now,
        now,
        now,
    };

    int i = 0;

    while(i < operations_v.size()){
        operations.emplace(_self, [&](auto & noperation){
            noperation.id = id_v[i];
            noperation.operation = operations_v[i];
            noperation.contract = contracts_v[i];
            noperation.pause = 0;
            noperation.period = delay_v[i];
            noperation.timestamp = timestamp_v[i];
        });
        i++;
    }
}


ACTION scheduler::configop(name id, name action, name contract, uint64_t period, uint64_t starttime) {
    require_auth(_self);

    auto itr = operations.find(id.value);
    
    uint64_t now = current_time_point().sec_since_epoch();

    check(starttime == 0 || starttime >= now, "Start time cannot be in the past. Specify start time of 0 to start now.");

    uint64_t start = (starttime == 0) ? now : starttime;
    

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
            noperation.period = period;
            noperation.timestamp = start - period;
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
    require_auth(get_self());

    print("Confirm the execution of " + operation.to_string());

    auto itr = operations.find(operation.value);
    check(itr != operations.end(), "Operation does not exist");

    operations.modify(itr, _self, [&](auto & moperation) {
        moperation.timestamp = current_time_point().sec_since_epoch();
    });
}


ACTION scheduler::execute() {
   // require_auth(_self);

   // print("Executing...");

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

    // =======================
    // cancel deferred execution if any
    // =======================

    cancel_deferred(contracts::scheduler.value);

    // =======================
    // execute operations
    // =======================

    auto ops_by_last_executed = operations.get_index<"bytimestamp"_n>();
    auto itr = ops_by_last_executed.begin();
    bool has_executed = false;

    while(itr != ops_by_last_executed.end()) {
        if(is_ready_to_execute(itr -> id)){

            print("\nOperation to be executed: " + itr -> id.to_string());

            exec_op(itr->id, itr->contract, itr->operation);

            has_executed = true;
            
            break;
        }
        itr++;
    }

    // =======================
    // schedule next execution
    // =======================

    auto it_s = has_executed ? 1 : config.get(seconds_to_execute.value, (contracts::scheduler.to_string() + ": the parameter " + seconds_to_execute.to_string() + " is not configured in " + contracts::settings.to_string()).c_str()).value;

    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "execute"_n,
        std::make_tuple()
    );

    print("Creating the deferred transaction...");

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = it_s;
    tx.send(contracts::scheduler.value /*eosio::current_time_point().sec_since_epoch() + 30*/, _self);
    

}

void scheduler::cancel_exec() {
    require_auth(get_self());
    cancel_deferred(contracts::scheduler.value);
}

ACTION scheduler::test1() {
    require_auth(get_self());

    name testname = "unit.test.1"_n;

    auto itr = test.find(testname.value);
    
    if( itr != test.end() ){
        test.modify(itr, _self, [&](auto & item) {
            item.value += 1;
        });
    }
    else{
        test.emplace(_self, [&](auto & item) {
            item.param = testname;
            item.value = 0;
        });
    }

}

ACTION scheduler::test2() {
    require_auth(get_self());

    name testname = "unit.test.2"_n;

    auto itr = test.find(testname.value);
    
    if(itr != test.end()){
        test.modify(itr, _self, [&](auto & item) {
            item.value += 1;
        });
    }
    else{
        test.emplace(_self, [&](auto & item) {
            item.param = testname;
            item.value = 0;
        });
    }

}

ACTION scheduler::testexec(name op) {
    require_auth(get_self());
    auto operation = operations.get(op.value, "op not found");
    exec_op(op, operation.contract, operation.operation);

}
void scheduler::exec_op(name id, name contract, name operation) {
    
    action a = action(
        permission_level{contract, "execute"_n},
        contract,
        operation,
        std::make_tuple()
    );

    // transaction txa;
    // txa.actions.emplace_back(a);
    // txa.delay_sec = 0;
    // txa.send(eosio::current_time_point().sec_since_epoch() + 20, _self);

    a.send();

    action c = action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "confirm"_n,
        std::make_tuple(id)
    );

    c.send();
}


EOSIO_DISPATCH(scheduler,(configop)(execute)(reset)(confirm)(pauseop)(removeop)(stop)(start)(test1)(test2)(testexec));
