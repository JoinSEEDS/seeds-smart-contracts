#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CONFIG_TABLE TABLE config_table { \
        name param; \
        uint64_t value; \
        string description; \
        name impact; \
\
        uint64_t primary_key()const { return param.value; } \
      }; 

#define DEFINE_CONFIG_TABLE_MULTI_INDEX typedef eosio::multi_index<"config"_n, config_table> config_tables; 

#define DEFINE_CONFIG_GET \
        uint64_t config_get (name key) { \
            auto citr = config.find(key.value);\
            if (citr == config.end()) { \
                eosio::check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());\
            }\
            return citr->value;\
        } \
