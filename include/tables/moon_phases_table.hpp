#include <eosio/eosio.hpp>

using namespace eosio;

#define DEFINE_MOON_PHASES_TABLE TABLE moon_phases_table { \
      uint64_t timestamp; \
      time_point time; \
      string phase_name; \
      string eclipse; \
\
      uint64_t primary_key() const { return timestamp; } \
    }; \

#define DEFINE_MOON_PHASES_TABLE_MULTI_INDEX typedef eosio::multi_index <"moonphases"_n, moon_phases_table> moon_phases_tables; \
