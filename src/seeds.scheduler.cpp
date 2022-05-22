#include <seeds.scheduler.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <string>


uint64_t scheduler::is_ready_op (const name & operation, const uint64_t & timestamp) {

    auto itr = operations.find(operation.value);

    if(itr -> pause > 0) {
        print(" transaction " + operation.to_string() + " is paused");
        return 0;
    }

    uint64_t periods = 0;

    periods = (timestamp - itr -> timestamp) / itr -> period;

    print("\nPERIODS: " + std::to_string(periods) + ", current_time: " + std::to_string(timestamp) + ", last_timestap: " + std::to_string(itr->timestamp) );

    return periods > 0 ? timestamp : 0;
    
}

uint64_t scheduler::is_ready_moon_op (const name & operation, const uint64_t & timestamp) {

    auto mitr = moonops.find(operation.value);

    if (mitr->pause > 0) {
        print("moon op " + operation.to_string() + " is paused");
        return 0;
    }

    uint64_t moon_timestamp = 0;

    if (mitr->start_time > mitr->last_moon_cycle_id) {
        moon_timestamp = mitr->start_time;
    } else {
        auto mpitr = moonphases.find(mitr->last_moon_cycle_id);
        std::advance(mpitr, mitr->quarter_moon_cycles);
        moon_timestamp = mpitr->timestamp;
    }

    return timestamp >= moon_timestamp ? moon_timestamp : 0;

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
    if (destructive) {
        auto itr = moonops.begin();
        while(itr != moonops.end()) {
            itr = moonops.erase(itr);
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

        name("org.appuses"),
        name("org.rankapps"),

        name("hstry.ptrxs")
        name("onbrd.clean"),
        name("hstry.ptrxs"),

        name("dao.cleanvts"),
        name("dao.calcdist")
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

        name("calcmappuses"),
        name("rankappuses"),

        name("cleanptrxs")
        name("chkcleanup"),
        name("cleanptrxs"),

        name("dhocleanvts"),
        name("dhocalcdists")
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

        contracts::organization,
        contracts::organization,

        contracts::history
        contracts::onboarding,
        contracts::history,

        contracts::dao,
        contracts::dao
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

        utils::seconds_per_day,
        utils::seconds_per_day,

        utils::seconds_per_day
        utils::seconds_per_day,
        utils::seconds_per_day,

        utils::seconds_per_day,
        utils::seconds_per_hour
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
        now,

        now,
        now + 600 - utils::seconds_per_hour, // kicks off 10 minutes later
        
        now,
        now,

        now,
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

    // Moon Operations
    // All moon operations need to be preserved, so they need to be added/removed manually.
    // use addmoonop
    // example below
    /**
    cleos -u "https://test.hypha.earth" push action cycle.seeds addmoonop '{ 
            "id":"inc.price",
            "action":"incprice",
            "contract":"tlosto.seeds",
            "quarter_moon_cycles":"1",
            "start_phase_name":"Full Moon"
    }' -p cycle.seeds@active
    */

}


ACTION scheduler::configop(name id, name action, name contract, uint64_t period, uint64_t starttime) {
    require_auth(_self);

    auto mop_itr = moonops.find(id.value);
    check(mop_itr == moonops.end(), "op must have unique name, op exists in moon operations table");

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

ACTION scheduler::addmoonop(name id, name action, name contract, uint64_t quarter_moon_cycles, string start_phase_name) {
    require_auth(_self);

    check(quarter_moon_cycles <= 4 && quarter_moon_cycles > 0, "invalid quarter moon cycles, it should be greater than zero and less or equals to 4");

    check(start_phase_name == "New Moon" ||
        start_phase_name == "First Quarter" ||
        start_phase_name == "Last Quarter" ||
        start_phase_name == "Full Moon", "start_phase_name must be one of New Moon, First Quarter, Last Quarter, Full Moon");

    uint64_t now = eosio::current_time_point().sec_since_epoch();

    uint64_t starttime = 0;

    auto mitr = moonphases.upper_bound(now);
    uint8_t count = 0;
    while (mitr != moonphases.end() && count < 4) {
        if (mitr->phase_name == start_phase_name) {
            starttime = mitr->timestamp;
            break;
        }
        mitr++;
        count++;
    }

    check(starttime != 0, "Unable to find moon phase. Must be one of New Moon, First Quarter, Last Quarter, Full Moon");

    configmoonop(id, action, contract, quarter_moon_cycles, starttime);
}

ACTION scheduler::configmoonop(name id, name action, name contract, uint64_t quarter_moon_cycles, uint64_t starttime) {
    require_auth(_self);

    check(quarter_moon_cycles <= 4 && quarter_moon_cycles > 0, "invalid quarter moon cycles, it should be greater than zero and less or equals to 4");
    
    uint64_t now = eosio::current_time_point().sec_since_epoch();
    check(starttime >= now, "start time must be a valid moon phase in the future");

    auto mitr = moonphases.find(starttime);
    check(mitr != moonphases.end(), "start time must be a valid moon phase timestamp");



    auto oitr = operations.find(id.value);
    check(oitr == operations.end(), "moon op must have unique name, op exists in normal operations table");

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
    if (itr != operations.end()) {
        operations.erase(itr);
        return;
    }

    auto mitr = moonops.find(id.value);
    if (mitr != moonops.end()) {
        moonops.erase(mitr);
        return;
    }

    check(false, contracts::scheduler.to_string() + ": the operation " + id.to_string() + " does not exist");
}

ACTION scheduler::pauseop(name id, uint8_t pause) {
    require_auth(get_self());

    auto itr = operations.find(id.value);
    if (itr != operations.end()) {
        operations.modify(itr, _self, [&](auto & moperation) {
            moperation.pause = pause;
        });
        return;
    }

    auto mitr = moonops.find(id.value);
    if (mitr != moonops.end()) {
        moonops.modify(mitr, _self, [&](auto & moonop){
            moonop.pause = pause;
        });
        return;
    }

    check(false, contracts::scheduler.to_string() + ": the operation " + id.to_string() + " does not exist");
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

    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();

    while(itr != ops_by_last_executed.end()) {
        if(is_ready_op(itr -> id, timestamp)){

            print("\nOperation to be executed: " + itr -> id.to_string(), "\n");

            exec_op(itr->id, itr->contract, itr->operation);

            ops_by_last_executed.modify(itr, _self, [&](auto & operation) {
                operation.timestamp = timestamp;
            });

            has_executed = true;
            
            break;
        }
        itr++;
    }

    if (!has_executed) {
        auto moonops_by_last_cycle = moonops.get_index<"bylastcycle"_n>();
        auto mitr = moonops_by_last_cycle.begin();
        
        while (mitr != moonops_by_last_cycle.end()) {
            uint64_t used_timestamp = is_ready_moon_op(mitr->id, timestamp);
            if (used_timestamp) {
                print("\nMoon operation to be executed: " + mitr->id.to_string(), "\n");

                exec_op(mitr->id, mitr->contract, mitr->action);

                moonops_by_last_cycle.modify(mitr, _self, [&](auto & operation){
                    operation.last_moon_cycle_id = used_timestamp;
                });

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

ACTION scheduler::checknext() {

    require_auth(get_self());

    // =======================
    // check operations
    // =======================

    auto ops_by_last_executed = operations.get_index<"bytimestamp"_n>();
    auto itr = ops_by_last_executed.begin();
    bool has_executed = false;

    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();

    while(itr != ops_by_last_executed.end()) {
        print(" checking op " + (itr -> id).to_string());
        if(is_ready_op(itr -> id, timestamp)){

            print(" next op: " + (itr -> id).to_string());

            has_executed = true;
            
            break;
        }
        itr++;
    }

    if (!has_executed) {

        auto moonops_by_last_cycle = moonops.get_index<"bylastcycle"_n>();
        auto mitr = moonops_by_last_cycle.begin();

        print(" checking moon op " + (mitr -> id).to_string());
        
        while (mitr != moonops_by_last_cycle.end()) {
            uint64_t used_timestamp = is_ready_moon_op(mitr->id, timestamp);
            if (used_timestamp) {
                
                print(" next op: " + (mitr -> id).to_string());
                
                has_executed = true;
                break;
            }
            mitr++;
        }
    }


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
    auto oitr = operations.find(op.value);
    if (oitr != operations.end()) {
        exec_op(op, oitr->contract, oitr->operation);
    } else {
        auto moonop = moonops.get(op.value, "op not found");
        exec_op(op, moonop.contract, moonop.action);
    }
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
}

// not using this
uint64_t scheduler::next_valid_moon_phase(uint64_t moon_cycle_id, uint64_t quarter_moon_cycles) {
    uint64_t now = eosio::current_time_point().sec_since_epoch();
    uint64_t result = moon_cycle_id;

    if (now > result) {
        auto mpitr = moonphases.find(result);
        while(now > result && mpitr != moonphases.end()) {
            std::advance(mpitr, quarter_moon_cycles);
            result = mpitr->timestamp;
        }
    }
    return moon_cycle_id;
}


EOSIO_DISPATCH(scheduler,
    (configop)(configmoonop)(addmoonop)
    (execute)(reset)(pauseop)(removeop)
    (stop)(start)(moonphase)(test1)(test2)(testexec)(updateops)
    (checknext)
);
