#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;

TABLE balance_table {
    name account;
    asset planted;
    asset reward;
    
    uint64_t primary_key()const { return account.value; }
    uint64_t by_planted()const { return planted.amount; }
};

typedef eosio::multi_index<"balances"_n, balance_table,
indexed_by<"byplanted"_n,
const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>>
> balance_tables;
