#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using std::string;

#define DEFINE_VOTE_TABLE TABLE vote_table { \
      uint64_t proposal_id; \
      name account; \
      uint64_t amount; \
      bool favour; \
      uint64_t primary_key()const { return account.value; } \
    };

#define DEFINE_VOTE_TABLE_MULTI_INDEX typedef eosio::multi_index<"votes"_n, vote_table> votes_tables;
