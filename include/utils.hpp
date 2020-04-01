#include <eosio/eosio.hpp>

namespace utils {

  eosio::symbol seeds_symbol = eosio::symbol("SEEDS", 4);

  bool is_valid_majority(uint64_t favour, uint64_t against, uint64_t majority) {
    return favour >= (favour + against) * majority / 100;
  }

  bool is_valid_quorum(uint64_t voters_number, uint64_t quorum, uint64_t total_number) {
    uint64_t voted_percentage = voters_number * 100 / total_number;
    return voted_percentage >= quorum;
  }

  void check_asset(eosio::asset quantity) {
    eosio::check(quantity.is_valid(), "invalid asset");
    eosio::check(quantity.symbol == seeds_symbol, "invalid asset");
  }

}


