#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_HARVEST_TABLE TABLE harvest_table {\
      name account;\
\
      uint64_t planted_score;\
      uint64_t planted_timestamp;\
\
      uint64_t transactions_score;\
      uint64_t tx_timestamp;\
\
      uint64_t reputation_score;\
      uint64_t rep_timestamp;\
\
      uint64_t community_building_score;\
      uint64_t community_building_timestamp;\
      \
      uint64_t contribution_score;\
      uint64_t contrib_timestamp;\
\
      uint64_t primary_key()const { return account.value; }\
      \
      uint64_t by_cs_points()const { return ( (planted_score + transactions_score + community_building_score) * reputation_score * 2) / 100; }\
\
    };
