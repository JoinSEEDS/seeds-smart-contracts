#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_KV_STORE_TABLE \
      TABLE kv_store_table { \
        name param; \
        uint64_t value; \
        uint64_t primary_key()const { return param.value; } \
      }; \
\
      typedef eosio::multi_index<"kvstore"_n, kv_store_table> kv_store_tables; \
\
      kv_store_tables kvstore;\


// make some methods to access kv store and set / get the current iterator value, defaulgint to begin

  // const_iterator start_itr_refs() {
  //   auto current_index = kvstore.find("ix.refs.1");
  //   if (current_index == kvstore.end()) {
  //     return refs.begin();
  //   } else {
  //     return refs.find(current_index->value);
  //   }
  // }