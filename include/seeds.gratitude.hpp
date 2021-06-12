#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>

using namespace eosio;
using namespace utils;
using std::string;
using std::vector;

CONTRACT gratitude : public contract {

  public:

    using contract::contract;
    gratitude(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        acks(receiver, receiver.value),
        stats(receiver, receiver.value),
        sizes(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value),
        config(contracts::settings, contracts::settings.value)
        {}

    ACTION reset();

    // Directly give gratitude to another user
    ACTION give(name from, name to, asset quantity, string memo);

    // Acknowledge gratitude to another user (will actually give tokens in proportion on new round)
    ACTION acknowledge(name from, name to, string memo);

    // Generates a new gratitude round, regenerating gratitude and splitting stored SEEDS
    ACTION newround();

    // Called after all acks are calculated
    ACTION payround();

    // Recursivelly calculate acks
    ACTION calcacks(uint64_t start);

    // For stats migration
    ACTION migratestats();

    // Calculate acks for testing
    ACTION testacks();

    // Called when depositing SEEDS into the pot
    ACTION deposit(name from, name to, asset quantity, string memo);


  private:

    void check_user(name account);
    void init_balances(name account);
    void _calc_acks(name account);
    void add_gratitude(name account, asset quantity);
    void sub_gratitude(name account, asset quantity);
    uint64_t get_current_volume();
    void update_stats(name from, name to, asset quantity);
    void _transfer(name beneficiary, asset quantity, string memo);
    uint64_t config_get(name key);
    void size_change(name id, int delta);
    void size_set(name id, uint64_t newsize);
    uint64_t get_size(name id);

    symbol gratitude_symbol = symbol("GRATZ", 4);
    inline void check_asset(asset quantity) {
      check(quantity.is_valid(), "invalid asset");
      check(quantity.symbol == gratitude_symbol, "invalid asset");
    }

    DEFINE_USER_TABLE

    DEFINE_USER_TABLE_MULTI_INDEX

    DEFINE_CONFIG_TABLE

    DEFINE_CONFIG_TABLE_MULTI_INDEX

    DEFINE_SIZE_TABLE

    DEFINE_SIZE_TABLE_MULTI_INDEX

    TABLE balance_table {
      name account;
      asset remaining; // Can only give the remaining gratitude
      asset received; // Stores the received gratitude

      uint64_t primary_key() const { return account.value; }
      uint64_t by_received() const { return received.amount; }
    };

    TABLE acks_table {
      name donor;
      vector<name> receivers; // can have duplicates

      uint64_t primary_key() const { return donor.value; }
    };

    TABLE stats_table {
      uint64_t round_id;
      uint64_t num_transfers;
      uint64_t num_acks;
      asset round_pot;
      asset volume;

      uint64_t primary_key() const { return round_id; }
    };

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byreceived"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_received>>
    > balance_tables;

    typedef eosio::multi_index<"acks"_n, acks_table> acks_tables;

    typedef eosio::multi_index<"stats"_n, stats_table> stats_tables;

    balance_tables balances;
    acks_tables acks;
    stats_tables stats;

    // External tables
    user_tables users;
    config_tables config;
    size_tables sizes;

    const name gratzgen_res = "gratz1.gen"_n; // Gratitude generated per cycle for residents
    const name gratzgen_cit = "gratz2.gen"_n; // Gratitude generated per cycle for citizens
    const name gratz_acks = "gratz.acks"_n; // Gratitude generated per cycle for citizens
    const name gratz_potkp = "gratz.potkp"_n; // Percentil of the pot to keep each round
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<gratitude>(name(receiver), name(code), &gratitude::deposit);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(gratitude, 
          (reset)
          (give)
          (acknowledge)
          (newround)
          (payround)
          (calcacks)
          (testacks)
          (migratestats)
        )
      }
  }
}

