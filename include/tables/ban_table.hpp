#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_BAN_TABLE TABLE ban_table { \
        name account; \
\
        uint64_t primary_key()const { return account.value; } \
      }; 

#define DEFINE_BAN_TABLE_MULTI_INDEX typedef eosio::multi_index<"ban"_n, ban_table> ban_tables; 
