#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_USER_TABLE TABLE         TABLE user_table { \
        name account; \
        name status; \
        name type; \
        string nickname; \
        string image; \
        string story; \
        string roles; \
        string skills; \
        string interests; \
        uint64_t reputation; \
        uint64_t timestamp; \
\
        uint64_t primary_key()const { return account.value; } \
        uint64_t by_reputation()const { return reputation; } \
      }; \



#define DEFINE_USER_TABLE_MULTI_INDEX typedef eosio::multi_index<"users"_n, user_table, \
      indexed_by<"byreputation"_n, \
      const_mem_fun<user_table, uint64_t, &user_table::by_reputation>> \
    > user_tables; \
