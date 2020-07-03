#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CONFIG_TABLE TABLE config_table { \
            name param; \
            uint64_t value; \
            uint64_t primary_key()const { return param.value; } \
        };

#define DEFINE_CONFIG_TABLE_MULTI_INDEX typedef eosio::multi_index<"config"_n, config_table> config_tables;
