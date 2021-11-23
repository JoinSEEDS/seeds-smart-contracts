#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <tables/rep_table.hpp>
#include <tables/size_table.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/config_float_table.hpp>
#include <tables/deferred_id_table.hpp>

using namespace eosio;
using std::string;

namespace utils {
  const uint64_t seconds_per_day = 86400;
  const uint64_t seconds_per_minute = 60;
  const uint64_t seconds_per_hour = seconds_per_minute * 60;
  const uint64_t moon_cycle = seconds_per_day * 29 + seconds_per_day / 2;
  const uint64_t proposal_cycle = moon_cycle;

  symbol seeds_symbol = symbol("SEEDS", 4);

  inline uint64_t linear_rank(uint64_t current, uint64_t total) { 
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
     * 
     * The count rebalances the next time we go over it, it's a dynamic system. 

     * The cheap way to fix it is to limit rank to 99
    */
    uint64_t r = (current * 100) / total; 
    if (r > 99) return 99;
    return r;
  }

  inline uint64_t spline_rank(uint64_t current, uint64_t total) {
    // Spline rank table coeficients
    const float rank_coefs[100] = { 0.00,
                            0.00,
                            0.01,
                            0.03,
                            0.06,
                            0.09,
                            0.14,
                            0.19,
                            0.26,
                            0.34,
                            0.44,
                            0.55,
                            0.67,
                            0.81,
                            0.97,
                            1.15,
                            1.34,
                            1.56,
                            1.80,
                            2.06,
                            2.34,
                            2.65,
                            2.98,
                            3.34,
                            3.72,
                            4.14,
                            4.58,
                            5.06,
                            5.57,
                            6.11,
                            6.68,
                            7.30,
                            7.94,
                            8.63,
                            9.35,
                            10.12,
                            10.92,
                            11.77,
                            12.66,
                            13.60,
                            14.59,
                            15.62,
                            16.70,
                            17.83,
                            19.02,
                            20.26,
                            21.55,
                            22.90,
                            24.30,
                            25.77,
                            27.23,
                            28.70,
                            30.16,
                            31.63,
                            33.09,
                            34.56,
                            36.02,
                            37.49,
                            38.95,
                            40.41,
                            41.88,
                            43.34,
                            44.81,
                            46.27,
                            47.74,
                            49.20,
                            50.67,
                            52.13,
                            53.60,
                            55.06,
                            56.53,
                            57.99,
                            59.45,
                            60.92,
                            62.38,
                            63.85,
                            65.31,
                            66.78,
                            68.24,
                            69.71,
                            71.17,
                            72.64,
                            74.10,
                            75.57,
                            77.03,
                            78.50,
                            79.96,
                            81.42,
                            82.89,
                            84.35,
                            85.82,
                            87.28,
                            88.75,
                            90.21,
                            91.68,
                            93.14,
                            94.61,
                            96.07,
                            97.54,
                            99.00
    };
    uint64_t calc = (current * 100) / total;
    if (calc > 99) calc = 99;

    return (uint64_t)rank_coefs[calc];
  }

  inline bool is_valid_majority(uint64_t favour, uint64_t against, uint64_t majority) {
    return favour >= (favour + against) * majority / 100;
  }

  inline bool is_valid_quorum(uint64_t voters_number, uint64_t quorum, uint64_t total_number) {
    if (total_number == 0) { return false; }
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

    DEFINE_USER_TABLE
    DEFINE_USER_TABLE_MULTI_INDEX

    user_tables users(contracts::accounts, contracts::accounts.value);
    auto uitr = users.find(account.value);
    name scope;

    if (uitr == users.end()) { return 0; }

    if (uitr->type == "individual"_n) {
      scope = contracts::accounts;
    } else if (uitr->type == "organisation"_n) {
      scope = "org"_n;
    }
    
    rep_tables rep(contracts::accounts, scope.value);
    
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

  uint64_t get_beginning_of_day_in_seconds() {
    auto sec = eosio::current_time_point().sec_since_epoch();
    auto date = eosio::time_point_sec(sec / 86400 * 86400);
    return date.utc_seconds;
  }

  template <typename T>
  inline void delete_table (const name & code, const uint64_t & scope) {

    T table(code, scope);
    auto itr = table.begin();

    while (itr != table.end()) {
      itr = table.erase(itr);
    }

  }

  template <typename... T>
  inline void send_deferred_transaction (
    const name & code,
    const permission_level & permission,
    const name & contract, 
    const name & action,  
    const std::tuple<T...> & data) {

    DEFINE_DEFERRED_ID_TABLE
    DEFINE_DEFERRED_ID_SINGLETON

    deferred_id_tables deferredids(code, code.value);
    deferred_id_table d_s = deferredids.get_or_create(code, deferred_id_table());

    d_s.id += 1;

    deferredids.set(d_s, code);

    transaction trx{};

    trx.actions.emplace_back(
      permission,
      contract,
      action,
      data
    );

    trx.delay_sec = 1;
    trx.send(d_s.id, code);

  }


}


