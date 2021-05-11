#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <string>


bool scheduler::is_ready_to_execute(name operation, name optype){
    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();

    if (optype == "op"_n) {
        auto itr = operations.find(operation.value);

        if (itr == operations.end()) {
            return false;
        }
        if(itr -> pause > 0) {
            print("transaction " + operation.to_string() + " is paused");
            return false;
        }

        uint64_t periods = 0;

        periods = (timestamp - itr -> timestamp) / itr -> period;

        print("\nPERIODS: " + std::to_string(periods) + ", current_time: " + std::to_string(timestamp) + ", last_timestap: " + std::to_string(itr->timestamp) );

        if(periods > 0) return true;
        return false;
    }
    else {
        auto mitr = moonops.find(operation.value);

        if (mitr == moonops.end()) {
            return false;
        }
        if (mitr->pause > 0) {
            print("moon op " + operation.to_string() + " is paused");
            return false;
        }

        uint64_t date = std::max(
            mitr->start_time, 
            mitr->last_moon_cycle_id + (mitr->quarter_moon_cycles * (utils::moon_cycle / 4))
        );


    }
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
    reset_aux(true);
}

ACTION scheduler::updateops() {
    reset_aux(false);
}

void scheduler::reset_aux(bool destructive) {
    require_auth(get_self());

    cancel_exec();

    auto itr = operations.begin();
    while(itr != operations.end()){
        if (destructive || !should_preserve_op(itr->id)) {
            print(" erase "+itr->id.to_string());
            itr = operations.erase(itr);
        } else {
            print(" preserve "+itr->id.to_string());
            itr++;
        }
    }

    auto titr = test.begin();
    while(titr != test.end()){
        titr = test.erase(titr);
    }

    std::vector<name> id_v = { 
        name("exch.period"),
        name("tokn.resetw"),

        name("acct.rankrep"),
        name("acct.rorgrep"),
        name("acct.rankcbs"),
        name("acct.rorgcbs"),

        name("hrvst.ranktx"),
        name("hrvst.rankpl"),

        name("hrvst.calccs"), // after the above 4
        name("hrvst.rankcs"), 
        name("hrvst.rorgcs"),
        name("hrvst.calctx"), // 24h
        name("hrvst.rgncs"),

        name("org.clndaus"),
        name("org.rankregn"),

        name("hrvst.orgtxs"),

        name("prop.dvoices"),

        name("forum.rank"),
        name("forum.giverp"),

        name("hrvst.qevs"),
        name("hrvst.mintr"),
        name("hrvst.hrvst"),
    };
    
    std::vector<name> operations_v = {
        name("onperiod"),
        name("resetweekly"),

        name("rankreps"),
        name("rankorgreps"),
        name("rankcbss"),
        name("rankorgcbss"),
        
        name("ranktxs"),
        name("rankplanteds"),

        name("calccss"),
        name("rankcss"),
        name("rankorgcss"),
        name("calctrxpts"),
        name("rankrgncss"),

        name("cleandaus"),
        name("rankregens"),

        name("rankorgtxs"),

        name("decayvoices"),

        name("rankforums"),
        name("givereps"),

        name("calcmqevs"),
        name("calcmintrate"),
        name("runharvest"),
    };

    std::vector<name> contracts_v = {
        contracts::exchange,
        contracts::token,

        contracts::accounts,
        contracts::accounts,
        contracts::accounts,
        contracts::accounts,

        contracts::harvest,
        contracts::harvest,

        contracts::harvest,
        contracts::harvest,
        contracts::harvest,
        contracts::harvest,
        contracts::harvest,

        contracts::organization,
        contracts::organization,

        contracts::harvest,

        contracts::proposals,

        contracts::forum,
        contracts::forum,

        contracts::harvest,
        contracts::harvest,
        contracts::harvest,
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

        utils::seconds_per_hour,
        utils::seconds_per_hour,
        utils::seconds_per_hour,
        utils::seconds_per_day,
        utils::seconds_per_day,

        utils::seconds_per_day / 2,
        utils::seconds_per_day,

        utils::seconds_per_day,

        utils::seconds_per_day,
        
        utils::moon_cycle / 4,
        utils::moon_cycle / 4,

        utils::seconds_per_day,
        utils::seconds_per_day,
        utils::seconds_per_hour,
    };

    uint64_t now = current_time_point().sec_since_epoch();

    std::vector<uint64_t> timestamp_v = {
        now,
        now,

        now - utils::seconds_per_hour, 
        now - utils::seconds_per_hour,
        now - utils::seconds_per_hour,
        now - utils::seconds_per_hour,

        now - utils::seconds_per_hour, 
        now - utils::seconds_per_hour, 

        now + 300 - utils::seconds_per_hour, // kicks off 5 minutes later
        now + 600 - utils::seconds_per_hour, // kicks off 10 minutes later
        now + 600 - utils::seconds_per_hour, // kicks off 10 minutes later
        now,
        now + 600 - utils::seconds_per_hour, // kicks off 10 minutes later

        now,
        now,
        now,

        now,

        now,

        now,
        now - utils::seconds_per_hour,

        now,
        now + 600 - utils::seconds_per_hour,
        now
    };

    int i = 0;

    while(i < operations_v.size()){
        auto oitr = operations.find(id_v[i].value);
        if (oitr == operations.end()) {
            print(" adding op "+id_v[i].to_string());
            operations.emplace(_self, [&](auto & noperation){
                noperation.id = id_v[i];
                noperation.operation = operations_v[i];
                noperation.contract = contracts_v[i];
                noperation.pause = 0;
                noperation.period = delay_v[i];
                noperation.timestamp = timestamp_v[i];
            });
        } else {
            print(" skipping op "+id_v[i].to_string());
        }
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

ACTION scheduler::configmoonop(name id, name action, name contract, uint64_t quarter_moon_cycles, uint64_t starttime) {
    require_auth(_self);

    check(quarter_moon_cycles <= 4 && quarter_moon_cycles > 0, "invalid quarter moon cycles, it should be greater than zero and less or equals to 4");
    
    auto mitr = moonphases.find(starttime);
    check(mitr != moonphases.end(), "start time must be a valid moon phase timestamp");

    auto mop_itr = moonops.find(id.value);

    if (mop_itr != moonops.end()) {
        moonops.modify(mop_itr, _self, [&](auto & op){
            op.action = action;
            op.contract = contract;
            op.quarter_moon_cycles = quarter_moon_cycles;
            op.start_time = starttime;
            op.pause = 0;
        });
    } else {
        moonops.emplace(_self, [&](auto & op){
            op.id = id;
            op.action = action;
            op.contract = contract;
            op.quarter_moon_cycles = quarter_moon_cycles;
            op.start_time = starttime;
            op.last_moon_cycle_id = 0;
            op.pause = 0;
        });
    }
}

ACTION scheduler::moonphase(uint64_t timestamp, string phase_name, string eclipse) {
    require_auth(_self);

    eosio::time_point_sec tpsec(timestamp);
    eosio::time_point time = tpsec;

    auto itr = moonphases.find(timestamp);
    
    //uint64_t now = current_time_point().sec_since_epoch();

    check(time.sec_since_epoch() == timestamp, "timestamp doesn't match time " + std::to_string(timestamp) + " vs " + std::to_string(time.sec_since_epoch())); 

    if(itr != moonphases.end()){
        moonphases.modify(itr, _self, [&](auto & item) {
            item.time = time;
            item.phase_name = phase_name;
            item.eclipse = eclipse;
        });
    }
    else{
        moonphases.emplace(_self, [&](auto & item) {
            item.timestamp = timestamp;
            item.time = time;
            item.phase_name = phase_name;
            item.eclipse = eclipse;
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
        if(is_ready_to_execute(itr -> id, "op"_n)){

            print("\nOperation to be executed: " + itr -> id.to_string(), "\n");

            exec_op(itr->id, itr->contract, itr->operation);

            has_executed = true;
            
            break;
        }
        itr++;
    }

    if (!has_executed) {
        auto moonops_by_last_cycle = moonops.get_index<"bylastcycle"_n>();
        auto mitr = moonops_by_last_cycle.begin();
        
        while (mitr != moonops_by_last_cycle.end()) {
            if (is_ready_to_execute(mitr->id, "moonop"_n)) {

                print("\nMoon operation to be executed: " + mitr->id.to_string(), "\n");

                exec_op(mitr->id, mitr->contract, mitr->action);

                has_executed = true;

                break;
            }
            mitr++;
        }
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


EOSIO_DISPATCH(scheduler,(configop)(execute)(reset)(confirm)(pauseop)(removeop)(stop)(start)(moonphase)(test1)(test2)(testexec)(updateops));
