#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CYCLE_TABLE TABLE cycle_table { \
        name cycle_id; \
        uint64_t ct; \
        uint64_t ix; \
        uint64_t timestamp; \
        uint64_t size; \
        uint64_t primary_key()const { return cycle_id.value; } \
      };

#define DEFINE_CYCLE_TABLE_MULTI_INDEX typedef eosio::multi_index<"cycle"_n, cycle_table> cycle_tables;
