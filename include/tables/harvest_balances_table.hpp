#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_PLANTED_BALANCES_TABLE TABLE balance_table { \
      name account; \
      asset planted; \
      uint64_t rank; \
\
      uint64_t primary_key()const { return account.value; } \
      uint128_t by_planted() const { return (uint128_t(planted.amount) << 64) + account.value; } \
      uint64_t by_rank() const { return rank; } \
\
    };\

#define DEFINE_PLANTED_BALANCES_TABLE_MULTI_INDEX typedef eosio::multi_index<"balances"_n, balance_table, \
        indexed_by<"byplanted"_n, \
        const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>> \
    > balance_tables; \
