#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/moon_phases_table.hpp>

using namespace eosio;
using std::string;


CONTRACT scheduler : public contract {

    public:
        using contract::contract;
        scheduler(name receiver, name code, datastream<const char*> ds)
        :contract(receiver, code, ds),
        operations(receiver, receiver.value),
        moonphases(receiver, receiver.value),
        test(receiver, receiver.value),
        moonops(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value)
        {}

        ACTION reset();

        ACTION updateops();

        ACTION execute();

        // specify start time any time in the future, or use 0 for "now"
        ACTION configop(name id, name action, name contract, uint64_t period, uint64_t starttime);

        ACTION configmoonop(name id, name action, name contract, uint64_t quarter_moon_cycles, uint64_t starttime);

        ACTION addmoonop(name id, name action, name contract, uint64_t quarter_moon_cycles, string start_phase_name);

        ACTION removeop(name id);

        ACTION pauseop(name id, uint8_t pause);
        
        ACTION stop();
        
        ACTION start();

        ACTION moonphase(uint64_t timestamp, string phase_name, string eclipse);

        ACTION test1();

        ACTION test2();

        ACTION testexec(name op);

    private:
        void exec_op(name id, name contract, name action);
        void cancel_exec();
        void reset_aux(bool destructive);
        uint64_t next_valid_moon_phase(uint64_t moon_cycle_id, uint64_t quarter_moon_cycles);
        bool should_preserve_op(name op_id) {
            return 
                op_id == "exch.period"_n || 
                op_id == "tokn.resetw"_n;
        }

        TABLE operations_table {
            name id;
            name operation;
            name contract;
            uint8_t pause;
            uint64_t period;
            uint64_t timestamp;

            uint64_t primary_key() const { return id.value; }
            uint64_t by_timestamp() const { return timestamp; }
        };

        DEFINE_MOON_PHASES_TABLE
        DEFINE_MOON_PHASES_TABLE_MULTI_INDEX

        TABLE moon_ops_table {
            name id;
            name action;
            name contract;
            uint64_t quarter_moon_cycles;
            uint64_t start_time;
            uint64_t last_moon_cycle_id;
            uint8_t pause;

            uint64_t primary_key() const { return id.value; }
            uint64_t by_last_cycle() const { return last_moon_cycle_id; }
        };

        TABLE test_table {
            name param;
            uint64_t value;
            uint64_t primary_key() const { return param.value; }
        };

        DEFINE_CONFIG_TABLE
        
        DEFINE_CONFIG_TABLE_MULTI_INDEX

        typedef eosio::multi_index < "operations"_n, operations_table,
            indexed_by<"bytimestamp"_n, 
            const_mem_fun<operations_table, uint64_t, &operations_table::by_timestamp>>
        > operations_tables;


        typedef eosio::multi_index <"moonops"_n, moon_ops_table,
            indexed_by<"bylastcycle"_n,
            const_mem_fun<moon_ops_table, uint64_t, &moon_ops_table::by_last_cycle>>
        > moon_ops_tables;

        typedef eosio::multi_index <"test"_n, test_table> test_tables;

        name seconds_to_execute = "secndstoexec"_n;

        operations_tables operations;
        config_tables config;
        test_tables test;
        moon_phases_tables moonphases;
        moon_ops_tables moonops;

        uint64_t is_ready_op(const name & operation, const uint64_t & timestamp);
        uint64_t is_ready_moon_op(const name & operation, const uint64_t & timestamp);
};
