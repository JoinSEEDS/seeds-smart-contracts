#include <eosio/eosio.hpp>

using eosio::name;

// SCOPE by name
#define DEFINE_RANKBOX_TABLE TABLE rankbox_table { \
        uint64_t rank; \
        uint64_t value; \
        uint64_t primary_key()const { return value; } \
        uint64_t by_rank()const { return rank; } \
      };

#define DEFINE_RANKBOX_TABLE_MULTI_INDEX \
        typedef eosio::multi_index<"rankbox"_n, rankbox_table, \
          indexed_by<"byrank"_n, const_mem_fun<rankbox_table, uint64_t, &rankbox_table::by_rank>> \
        > rankbox_tables;

