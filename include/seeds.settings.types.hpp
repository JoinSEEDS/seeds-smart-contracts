#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;

TABLE config_table {
    name param;
    uint64_t value;
    uint64_t primary_key()const { return param.value; }
};

typedef eosio::multi_index<"config"_n, config_table> config_tables;
