#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_DHO_SHARE_TABLE TABLE dho_share_table { \
      name dho; \
      double total_percentage; \
      double dist_percentage; \
\
      uint64_t primary_key() const { return dho.value; } \
    }; \

#define DEFINE_DHO_SHARE_TABLE_MULTI_INDEX typedef eosio::multi_index<"dhoshares"_n, dho_share_table> dho_share_tables; \
