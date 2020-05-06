#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <harvest_table.hpp>

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

  double rep_multiplier_for_score(uint64_t rep_score) {
    // rep is 0 - 99
    check(rep_score < 101, "illegal rep score ");
    // return 0 - 2
    return rep_score * 2.0 / 99.0; 
  }

  double get_rep_multiplier(name account) {

    DEFINE_HARVEST_TABLE
    eosio::multi_index<"harvest"_n, harvest_table> harvest(contracts::harvest, contracts::harvest.value);

    auto hitr = harvest.find(account.value);

    if (hitr == harvest.end()) {
      return 0;
    }

    return rep_multiplier_for_score(hitr->reputation_score);

  }

}


