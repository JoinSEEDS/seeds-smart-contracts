#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <tables/rep_table.hpp>
#include <tables/size_table.hpp>

using namespace eosio;

namespace utils {
  const uint64_t seconds_per_day = 86400;
  const uint64_t seconds_per_minute = 60;
  const uint64_t seconds_per_hour = seconds_per_minute * 60;
  const uint64_t moon_cycle = seconds_per_day * 29 + seconds_per_day / 2;
  const uint64_t proposal_cycle = moon_cycle / 2; // proposals run at half moon cycles at the moment

  symbol seeds_symbol = symbol("SEEDS", 4);

  inline uint64_t rank(uint64_t current, uint64_t total) { 
    /**
     * TODO - Table Locks
     * 
     * rank can exceed 99 when we count accounts double. This can happen when an account receives reputation while we iterate
     * and is moved to a new bucket.
     * We then count it again in this new bucket - this means we end up counting more users than are in the reps table
     * which skews the count.
     * The reason this is not atomic is that we go over the table in chunks, so inbetween chunks anything can happen.
     * 
     * The correct way to solve this would be to wait until the ranking is finished, and to apply changes in reputation only once that has
     * happened. Like a manually implemented table lock. (rep or any other number we are ranking)
     * 
     * For now that's too complex - a lock would have to be between changing reputation and ranking. Ranking could simply wait until there's no 
     * lock but applying reputation we would have to store changes in a secondary table rep_delta, apply all changes there while rep ranking is happening
     * and apply the delta back to the full table once rep ranking is finished.
     * 
     * The count rebalances the next time we go over it, it's a dynamic system. 
     * 
     * The cheap way to fix it is to limit rank to 99
    */

    uint64_t r = (current * 100) / total; 
    if (r > 99) return 99;
    return r;
  }

  inline bool is_valid_majority(uint64_t favour, uint64_t against, uint64_t majority) {
    return favour >= (favour + against) * majority / 100;
  }

  inline bool is_valid_quorum(uint64_t voters_number, uint64_t quorum, uint64_t total_number) {
    uint64_t voted_percentage = voters_number * 100 / total_number;
    return voted_percentage >= quorum;
  }

  inline void check_asset(asset quantity) {
    check(quantity.is_valid(), "invalid asset");
    check(quantity.symbol == seeds_symbol, "invalid asset");
  }

  inline double rep_multiplier_for_score(uint64_t rep_score) {
    // rep is 0 - 99
    check(rep_score < 101, "illegal rep score ");
    // return 0 - 2
    return rep_score * 2.0 / 99.0; 
  }

  double get_rep_multiplier(name account) {

    DEFINE_REP_TABLE
    DEFINE_REP_TABLE_MULTI_INDEX
    
    rep_tables rep(contracts::accounts, contracts::accounts.value);

    auto ritr = rep.find(account.value);

    if (ritr == rep.end()) {
      return 0;
    }

    return rep_multiplier_for_score(ritr->rank);

  }

  uint64_t get_users_size() {

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX
    
    size_tables size(contracts::accounts, contracts::accounts.value);

    return size.get("users.sz"_n.value, "users size unknown").size;

  }

}


