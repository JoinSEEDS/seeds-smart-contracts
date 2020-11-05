#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using std::string;

#define DEFINE_PRICE_HISTORY_TABLE TABLE price_history_table { \
      uint64_t id; \
      asset seeds_usd; \
      time_point date; \
      \
      uint64_t primary_key()const { return id; } \
    }; \

#define DEFINE_PRICE_HISTORY_TABLE_MULTI_INDEX typedef eosio::multi_index<"pricehistory"_n, price_history_table> price_history_tables;