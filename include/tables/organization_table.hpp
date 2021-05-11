#include <eosio/eosio.hpp>

using eosio::name;

#define DEFINE_ORGANIZATION_TABLE TABLE organization_table { \
            name org_name; \
            name owner; \
            uint64_t status; \
            int64_t regen; \
            uint64_t reputation; \
            uint64_t voice; \
            asset planted; \
\
            uint64_t primary_key() const { return org_name.value; } \
        }; \

#define DEFINE_ORGANIZATION_TABLE_MULTI_INDEX typedef eosio::multi_index <"organization"_n, organization_table> organization_tables; \

