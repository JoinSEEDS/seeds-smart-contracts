#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>

using namespace eosio;


CONTRACT scheduler : public contract {

    public:
        using contract::contract;
        scheduler(name receiver, name code, datastream<const char*> ds)
        :contract(receiver, code, ds),
        operations(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value)
        {}

        ACTION reset();

        ACTION execute();

        ACTION configop(name action, name contract, uint64_t period);

        ACTION noop();

    private:
        TABLE operations_table {
            name operation;
            name contract;
            uint64_t period;
            uint64_t timestamp;

            uint64_t primary_key() const { return operation.value; }
        };

        TABLE config_table {
            name param;
            uint64_t value;
            uint64_t primary_key() const { return param.value; }
        };


        typedef eosio::multi_index < "operations"_n, operations_table> operations_tables;

        typedef eosio::multi_index <"config"_n, config_table> config_tables;


        operations_tables operations;
        config_tables config;
};



