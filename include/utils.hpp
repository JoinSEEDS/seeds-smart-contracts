#include <eosio/eosio.hpp>

using namespace eosio;

namespace utils {

  const uint64_t seconds_per_day = 86400;
  const uint64_t moon_cycle = seconds_per_day * 29 + seconds_per_day / 2;

  symbol seeds_symbol = symbol("SEEDS", 4);

  bool is_valid_majority(uint64_t favour, uint64_t against, uint64_t majority) {
    return favour >= (favour + against) * majority / 100;
  }

  bool is_valid_quorum(uint64_t voters_number, uint64_t quorum, uint64_t total_number) {
    uint64_t voted_percentage = voters_number * 100 / total_number;
    return voted_percentage >= quorum;
  }

  void check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == seeds_symbol, "invalid asset");
  }

}


