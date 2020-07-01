#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_REP_TABLE TABLE rep_table { \
        name account; \
        uint64_t rep; \
\
        uint64_t primary_key() const { return account.value; } \
        uint64_t by_rep()const { return rep; } \
      };

#define DEFINE_REP_TABLE_MULTI_INDEX \
        typedef eosio::multi_index<"rep"_n, rep_table, \
          indexed_by<"byrep"_n, const_mem_fun<rep_table, uint64_t, &rep_table::by_rep>> \
        > rep_tables;
