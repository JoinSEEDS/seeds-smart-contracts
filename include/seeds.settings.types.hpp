#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;

#define IMPORT_SETTINGS_TYPES \
IMPORT_CONFIG_TABLE \
IMPORT_CONFIG_TABLES

#define IMPORT_CONFIG_TABLE TABLE config_table {\
    name param;\
    uint64_t value;\
    uint64_t primary_key()const { return param.value; }\
};

#define IMPORT_CONFIG_TABLES typedef eosio::multi_index<"config"_n, config_table> config_tables;
