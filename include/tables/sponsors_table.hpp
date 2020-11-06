#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using std::string;


#define DEFINE_SPONSORS_TABLE TABLE sponsors_table { \
      name    sponsor; \
      asset   locked_balance = asset (0, symbol("SEEDS", 4)); \
      asset   liquid_balance = asset (0, symbol("SEEDS", 4)); \
    \
      uint64_t primary_key() const { return sponsor.value; } \
  }; \

#define DEFINE_SPONSORS_TABLE_MULTI_INDEX typedef eosio::multi_index<"sponsors"_n, sponsors_table> sponsors_tables;