#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;
using std::string;

#define IMPORT_ACCOUNTS_TYPES \
IMPORT_USER_TABLE \
IMPORT_USER_TABLES \
IMPORT_REP_TABLE \
IMPORT_REP_TABLES \
IMPORT_APP_TABLE \
IMPORT_APP_TABLES \

#define IMPORT_USER_TABLE_ALL \
IMPORT_USER_TABLE \
IMPORT_USER_TABLES

#define IMPORT_APP_TABLE_ALL \
IMPORT_APP_TABLE \
IMPORT_APP_TABLES

#define IMPORT_REP_TABLE_ALL \
IMPORT_REP_TABLE \
IMPORT_REP_TABLES 

#define IMPORT_USER_TABLE TABLE user_table {\
    name account;\
    name status;\
    name type;\
    string nickname;\
    string image;\
    string story;\
    string roles;\
    string skills;\
    string interests;\
    uint64_t reputation;\
    uint64_t timestamp;\
    \
    uint64_t primary_key()const { return account.value; }\
    uint64_t by_reputation()const { return reputation; }\
};

#define IMPORT_REP_TABLE struct [[eosio::table]] rep_table {\
    name account;\
    uint64_t reputation;\
    \
    uint64_t primary_key() const { return account.value; }\
    uint64_t by_reputation()const { return reputation; }\
};

#define IMPORT_APP_TABLE struct [[eosio::table]]  app_table {\
    name account;\
\
    uint64_t primary_key()const { return account.value; }\
};\

#define IMPORT_REP_TABLES typedef eosio::multi_index<"reputation"_n, rep_table,\
indexed_by<"byreputation"_n,\
const_mem_fun<rep_table, uint64_t, &rep_table::by_reputation>>\
> rep_tables;\

#define IMPORT_USER_TABLES typedef eosio::multi_index<"users"_n, user_table,\
indexed_by<"byreputation"_n,\
const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>\
> user_tables;\

#define IMPORT_APP_TABLES typedef eosio::multi_index<"apps"_n, app_table> app_tables;\
