#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <seeds.token.hpp>

using namespace eosio;
using std::string;

CONTRACT harvest : public contract {
  public:
    using contract::contract;
    harvest(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        harveststat(receiver, receiver.value),
        config(name("seedsettings"), name("seedsettings").value),
        users(name("seedsaccnts3"), name("seedsaccnts3").value)
        {}

    ACTION reset();

    ACTION onperiod();

    ACTION plant(name from, name to, asset quantity, string memo);

    ACTION unplant(name from, asset quantity);

    ACTION claimrefund(name from, uint64_t request_id);

    ACTION cancelrefund(name from, uint64_t request_id);

    ACTION claimreward(name from, asset reward);

    ACTION sow(name from, name to, asset quantity);

    ACTION upbyplanted();

    using reset_action = action_wrapper<"reset"_n, &harvest::reset>;
    using onperiod_action = action_wrapper<"onperiod"_n, &harvest::onperiod>;
    using unplant_action = action_wrapper<"unplant"_n, &harvest::unplant>;
    using claimreward_action = action_wrapper<"claimreward"_n, &harvest::claimreward>;
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    uint64_t ONE_WEEK = 604800;

    void init_balance(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void deposit(asset quantity);
    void withdraw(name account, asset quantity);

    TABLE config_table {
      name param;
      uint64_t value;
      uint64_t primary_key()const { return param.value; }
    };

    TABLE balance_table {
      name account;
      asset planted;
      asset reward;

      uint64_t primary_key()const { return account.value; }
      uint64_t by_planted()const { return planted.amount; }
    };

    TABLE refund_table {
      uint64_t request_id;
      uint64_t refund_id;
      name account;
      asset amount;
      uint32_t weeks_delay;
      uint32_t request_time;

      uint64_t primary_key()const { return refund_id; }
    };

    TABLE user_table {
        name account;
        name status;
        uint64_t reputation;

        uint64_t primary_key()const { return account.value; }
        uint64_t by_reputation()const { return reputation; }
    };

    TABLE transaction_table {
        name account;
        asset transactions_volume;
        uint64_t transactions_number;

        uint64_t primary_key()const { return transactions_volume.symbol.code().raw(); }
        uint64_t by_transaction_volume()const { return transactions_volume.amount; }
    };

    TABLE harvest_table {
      name account;
      uint64_t planted_score;
      uint64_t transactions_score;
      uint64_t reputation_score;
      uint64_t contribution_score;

      uint64_t primary_key()const { return account.value; }
    };

    typedef eosio::multi_index<"refunds"_n, refund_table> refund_tables;

    typedef eosio::multi_index<"config"_n, config_table> config_tables;

    typedef eosio::multi_index<"harvest"_n, harvest_table> harvest_tables;

    typedef eosio::multi_index< "trxstat"_n, transaction_table,
        indexed_by<"bytrxvolume"_n,
        const_mem_fun<transaction_table, uint64_t, &transaction_table::by_transaction_volume>>
    > transaction_tables;

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byplanted"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>>
    > balance_tables;

    typedef eosio::multi_index<"users"_n, user_table,
        indexed_by<"byreputation"_n,
        const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>
    > user_tables;

    config_tables config;
    balance_tables balances;
    user_tables users;
    harvest_tables harveststat;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == "seedstoken12"_n.value) {
      execute_action<harvest>(name(receiver), name(code), &harvest::plant);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(harvest, (reset)(onperiod)(unplant)(claimreward)(sow)(upbyplanted))
      }
  }
}
