#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_PLANTED_TABLE TABLE planted_table { \
      name account; \
      asset planted; \
      uint64_t rank; \
\
      uint64_t primary_key()const { return account.value; } \
      uint128_t by_planted() const { return (uint128_t(planted.amount) << 64) + account.value; } \
      uint64_t by_rank() const { return rank; } \
    }; \

#define DEFINE_PLANTED_TABLE_MULTI_INDEX \
    typedef eosio::multi_index<"planted"_n, planted_table, \
      indexed_by<"byplanted"_n,const_mem_fun<planted_table, uint128_t, &planted_table::by_planted>>, \
      indexed_by<"byrank"_n,const_mem_fun<planted_table, uint64_t, &planted_table::by_rank>> \
    > planted_tables; \
