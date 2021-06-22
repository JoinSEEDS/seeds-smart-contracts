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
      uint64_t last_ran_cycle; \
      uint64_t age; \
      std::vector<uint64_t> cycles_per_status; \
      EOSLIB_SERIALIZE(referendum_table, (referendum_id)(favour)(against)(staked)(creator)(title) \
        (summary)(description)(image)(url)(created_at)(status)(stage)(type)(last_ran_cycle)(age)(cycles_per_status)) \
      \
      uint64_t primary_key() const { return referendum_id; } \
      uint128_t by_stage_id() const { return (uint128_t(stage.value) << 64) + referendum_id; } \
      uint128_t by_status_id() const { return (uint128_t(status.value) << 64) + referendum_id; } \
      uint128_t by_type_id() const { return (uint128_t(type.value) << 64) + referendum_id; } \
      uint128_t by_creator_id() const { return (uint128_t(creator.value) << 64) + referendum_id; } \
    };

#define DEFINE_REFERENDUM_TABLE_MULTI_INDEX \
    typedef eosio::multi_index<"referendums"_n, referendum_table, \
      indexed_by<"bystageid"_n, const_mem_fun<referendum_table, uint128_t, &referendum_table::by_stage_id>>, \
      indexed_by<"bystatusid"_n, const_mem_fun<referendum_table, uint128_t, &referendum_table::by_status_id>>, \
      indexed_by<"bytypeid"_n, const_mem_fun<referendum_table, uint128_t, &referendum_table::by_type_id>>, \
      indexed_by<"bycreatorid"_n, const_mem_fun<referendum_table, uint128_t, &referendum_table::by_creator_id>> \
    > referendum_tables;
