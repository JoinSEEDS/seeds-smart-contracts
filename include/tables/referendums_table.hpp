#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using std::string;

typedef std::variant<std::monostate, uint64_t, int64_t, double, name, asset, string, bool, eosio::time_point> VariantValue; 

#define DEFINE_REFERENDUM_TABLE TABLE referendum_table { \
      uint64_t referendum_id; \
      uint64_t favour; \
      uint64_t against; \
      asset staked; \
      name creator; \
      string title; \
      string summary; \
      string description; \
      string image; \
      string url; \
      time_point created_at; \
      name status; \
      name stage; \
      name type; \
      std::map<string, VariantValue> special_attributes; \
      \
      EOSLIB_SERIALIZE(referendum_table, (referendum_id)(favour)(against)(staked)(creator)(title) \
        (summary)(description)(image)(url)(created_at)(status)(stage)(type)(special_attributes)) \
      \
      uint64_t primary_key() const { return referendum_id; } \
    };

#define DEFINE_REFERENDUM_TABLE_MULTI_INDEX typedef eosio::multi_index<"referendums"_n, referendum_table> referendum_tables;
