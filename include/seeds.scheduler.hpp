#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>
#include <utils.hpp>

using namespace eosio;


CONTRACT scheduler : public contract {

    public:
        using contract::contract;
        scheduler(name receiver, name code, datastream<const char*> ds)
        :contract(receiver, code, ds),
        operations(receiver, receiver.value),
        test(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value)
        {}

        ACTION reset();

        ACTION execute();

        ACTION configop(name id, name action, name contract, uint64_t period);

        ACTION removeop(name id);

        ACTION pauseop(name id, uint8_t pause);

        ACTION confirm(name operation);

        ACTION cancelexec();

        ACTION test1();

        ACTION test2();

    private:
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

        TABLE test_table {
            name param;
            uint64_t value;
            uint64_t primary_key() const { return param.value; }
        };

        TABLE config_table {
            name param;
            uint64_t value;
            uint64_t primary_key() const { return param.value; }
        };


        typedef eosio::multi_index < "operations"_n, operations_table,
            indexed_by<"bytimestamp"_n, const_mem_fun<operations_table, uint64_t, &operations_table::by_timestamp>>
        > operations_tables;

        typedef eosio::multi_index <"config"_n, config_table> config_tables;
        typedef eosio::multi_index <"test"_n, test_table> test_tables;

        name seconds_to_execute = "secndstoexec"_n;

        operations_tables operations;
        config_tables config;
        test_tables test;

        bool is_ready_to_execute(name operation);
};
