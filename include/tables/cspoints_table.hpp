#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_CS_POINTS_TABLE TABLE cs_points_table { \
      name account; \
      uint32_t contribution_points; \
      uint64_t rank; \
\
      uint64_t primary_key() const { return account.value; } \
      uint64_t by_cs_points() const { return (uint64_t(contribution_points) << 32) +  ( (account.value <<32) >> 32) ; } \
      uint64_t by_rank() const { return rank; } \
    }; \

#define DEFINE_CS_POINTS_TABLE_MULTI_INDEX typedef eosio::multi_index<"cspoints"_n, cs_points_table, \
      indexed_by<"bycspoints"_n,const_mem_fun<cs_points_table, uint64_t, &cs_points_table::by_cs_points>>, \
      indexed_by<"byrank"_n,const_mem_fun<cs_points_table, uint64_t, &cs_points_table::by_rank>> \
    > cs_points_tables; \


        

    

