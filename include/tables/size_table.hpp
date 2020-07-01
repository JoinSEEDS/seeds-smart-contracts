#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_SIZE_TABLE TABLE size_table { \
        name id; \
        uint64_t size; \
        uint64_t primary_key()const { return id.value; } \
      };

#define DEFINE_SIZE_TABLE_MULTI_INDEX typedef eosio::multi_index<"sizes"_n, size_table> size_tables;
