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

CONTRACT gratitude : public contract {

  public:

    using contract::contract;
    gratitude(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        stats(receiver, receiver.value),
        sizes(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value),
        config(contracts::settings, contracts::settings.value)
        {}

    ACTION reset();

    // Give gratitude to another user
    ACTION give(name from, name to, asset quantity, string memo);

    // Generates a new gratitude round, regenerating gratitude and splitting stored SEEDS
    ACTION newround();

  private:

    void check_user(name account);
    void init_balances(name account);
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

    TABLE stats_table {
      uint64_t round_id;
      uint64_t num_transfers;
      asset volume;

      uint64_t primary_key() const { return round_id; }
    };

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byreceived"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_received>>
    > balance_tables;

    typedef eosio::multi_index<"stats"_n, stats_table> stats_tables;

    balance_tables balances;
    stats_tables stats;

    // External tables
    user_tables users;
    config_tables config;
    size_tables sizes;

    const name gratzgen = "gratz.gen"_n; // Gratitude generated per cycle setting
};

EOSIO_DISPATCH(gratitude, 
  (reset)(give)(newround)
);