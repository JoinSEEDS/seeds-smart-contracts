#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <harvest_table.hpp>
#include <cycle_table.hpp>
#include <utils.hpp>
#include <tables/rep_table.hpp>
#include <tables/size_table.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/cbs_table.hpp>
#include <cmath> 

using namespace eosio;
using namespace utils;
using std::string;

CONTRACT harvest : public contract {
  public:
    using contract::contract;
    harvest(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        harveststat(receiver, receiver.value),
        txpoints(receiver, receiver.value),
        cspoints(receiver, receiver.value),
        cycle(receiver, receiver.value),
        sizes(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value),
        users(contracts::accounts, contracts::accounts.value),
        rep(contracts::accounts, contracts::accounts.value),
        cbs(contracts::accounts, contracts::accounts.value)
        {}
        
    ACTION reset();

    ACTION plant(name from, name to, asset quantity, string memo);

    ACTION unplant(name from, asset quantity);

    ACTION claimrefund(name from, uint64_t request_id);

    ACTION cancelrefund(name from, uint64_t request_id);

    ACTION claimreward(name from);

    ACTION sow(name from, name to, asset quantity);

    ACTION runharvest();

    ACTION calcplanted(); // caluclate planted score

    ACTION calctrxpt(); // calculate transaction points
    ACTION calctrxpts(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION calctrx(); // calculate transaction score

    ACTION calccs(); // calculate contribution score

    ACTION updatetxpt(name account);


    ACTION payforcpu(name account);

    ACTION testreward(name from);
    ACTION testclaim(name from, uint64_t request_id, uint64_t sec_rewind);
    ACTION testupdatecs(name account, uint64_t contribution_score);

    ACTION clearscores();  // DEBUG REMOVE - migrate method

  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    uint64_t ONE_WEEK = 604800;

    name rep_rank_name = "rep.rnk"_n;
    name tx_rank_name = "tx.rnk"_n;
    name planted_rank_name = "planted.rnk"_n;
    name cbs_rank_name = "cbs.rnk"_n;
    name cs_rank_name = "cs.rnk"_n;

    void init_balance(name account);
    void init_harvest_stat(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void deposit(asset quantity);
    void withdraw(name account, asset quantity);
    void calc_tx_points(name account);
    double get_rep_multiplier(name account);

    void size_change(name id, int delta);
    void size_set(name id, uint64_t newsize);
    uint64_t get_size(name id);

    // Contract Tables

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

    TABLE tx_points_table {
      name account;
      uint64_t points;  // transaction points
      uint64_t cycle;   // calculation cycle - we will use this when we have too many users to do them all

      uint64_t primary_key() const { return account.value; }
      uint64_t by_points() const { return points; }
      uint64_t by_cycle() const { return cycle; }

    };

    typedef eosio::multi_index<"txpoints"_n, tx_points_table,
      indexed_by<"bypoints"_n,const_mem_fun<tx_points_table, uint64_t, &tx_points_table::by_points>>,
      indexed_by<"bycycle"_n,const_mem_fun<tx_points_table, uint64_t, &tx_points_table::by_cycle>>
    > tx_points_tables;

    TABLE cs_points_table {
        name account;
        uint64_t contribution_points;
        uint64_t cycle;

        uint64_t primary_key() const { return account.value; }
        uint64_t by_cs_points()const { return contribution_points; }
        uint64_t by_cycle()const { return cycle; }
    };

    typedef eosio::multi_index<"cspoints"_n, cs_points_table,
      indexed_by<"bycspoints"_n,const_mem_fun<cs_points_table, uint64_t, &cs_points_table::by_cs_points>>,
      indexed_by<"bycycle"_n,const_mem_fun<cs_points_table, uint64_t, &cs_points_table::by_cycle>>
    > cs_points_tables;

    DEFINE_SIZE_TABLE

    DEFINE_SIZE_TABLE_MULTI_INDEX

    // External Tables

    DEFINE_REP_TABLE

    DEFINE_REP_TABLE_MULTI_INDEX

    DEFINE_USER_TABLE

    DEFINE_USER_TABLE_MULTI_INDEX

    DEFINE_CONFIG_TABLE

    DEFINE_CONFIG_TABLE_MULTI_INDEX

    DEFINE_CBS_TABLE

    DEFINE_CBS_TABLE_MULTI_INDEX

    TABLE transaction_table { // from History contract - scoped by 'from'
       uint64_t id;
       name to;
       asset quantity;
       uint64_t timestamp;

       uint64_t primary_key() const { return id; }
       uint64_t by_timestamp() const { return timestamp; }
       uint64_t by_to() const { return to.value; }
       uint64_t by_quantity() const { return quantity.amount; }
    };

    DEFINE_HARVEST_TABLE

    DEFINE_CYCLE_TABLE

    DEFINE_CYCLE_TABLE_MULTI_INDEX

    typedef eosio::multi_index<"refunds"_n, refund_table> refund_tables;

    typedef eosio::multi_index<"harvest"_n, harvest_table> harvest_tables;

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byplanted"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>>
    > balance_tables;

    typedef eosio::multi_index<"transactions"_n, transaction_table,
      indexed_by<"bytimestamp"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_timestamp>>,
      indexed_by<"byquantity"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_quantity>>,
      indexed_by<"byto"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_to>>
    > transaction_tables;

    // Contract Tables
    balance_tables balances;
    tx_points_tables txpoints;
    harvest_tables harveststat;
    cs_points_tables cspoints;
    cycle_tables cycle;
    size_tables sizes;

    // External Tables
    config_tables config;
    user_tables users;
    cbs_tables cbs;
    rep_tables rep;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<harvest>(name(receiver), name(code), &harvest::plant);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(harvest, 
          (payforcpu)(reset)(runharvest)
          (unplant)(claimreward)(claimrefund)(cancelrefund)(sow)
          (calctrx)(calctrxpt)(calctrxpts)(calcplanted)(calccs)
          (updatetxpt)(clearscores)
          (testreward)(testclaim)(testupdatecs))
      }
  }
}
