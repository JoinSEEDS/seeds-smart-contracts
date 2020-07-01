#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CBS_TABLE TABLE cbs_table { \
        name account; \
        uint32_t community_building_score; \
        uint64_t rank; \
\
        uint64_t primary_key() const { return account.value; } \
        uint64_t by_cbs() const { return (uint64_t(community_building_score) << 32) +  ( (account.value <<32) >> 32) ; } \
        uint64_t by_rank() const { return rank; } \
      };

#define DEFINE_CBS_TABLE_MULTI_INDEX \
        typedef eosio::multi_index<"cbs"_n, cbs_table, \
          indexed_by<"bycbs"_n, const_mem_fun<cbs_table, uint64_t, &cbs_table::by_cbs>>, \
          indexed_by<"byrank"_n, const_mem_fun<cbs_table, uint64_t, &cbs_table::by_rank>> \
        > cbs_tables;
