#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using eosio::name;

#define DEFINE_DEFERRED_ID_TABLE TABLE deferred_id_table { \
  uint64_t id; \
};

#define DEFINE_DEFERRED_ID_SINGLETON typedef singleton<"deferredids"_n, deferred_id_table> deferred_id_tables; \
typedef eosio::multi_index<"deferredids"_n, deferred_id_table> dump_for_deferred_id;
