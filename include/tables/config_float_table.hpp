#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CONFIG_FLOAT_TABLE TABLE config_float_table { \
        name param; \
        double value; \
        string description; \
        name impact; \
\
        uint64_t primary_key()const { return param.value; } \
      }; 

#define DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX typedef eosio::multi_index<"configfloat"_n, config_float_table> config_float_tables; 
