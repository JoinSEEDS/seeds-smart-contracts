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
#include <tables/cspoints_table.hpp>
#include <eosio/singleton.hpp>
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
        planted(receiver, receiver.value),
        txpoints(receiver, receiver.value),
        cspoints(receiver, receiver.value),
        sizes(receiver, receiver.value),
        total(receiver, receiver.value),
        harveststat(receiver, receiver.value),
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

    ACTION sow(name from, name to, asset quantity);

    ACTION runharvest();

    ACTION rankplanteds();
    ACTION rankplanted(uint128_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION calctrxpts(); // calculate transaction points // 24h interval
    ACTION calctrxpt(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION ranktxs(); // rank transaction score // 1h interval
    ACTION ranktx(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION calccss(); // calculate contribution points // 1h inteval
    ACTION updatecs(name account); 
    ACTION calccs(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION rankcss(); // rank contribution score //
    ACTION rankcs(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION updatetxpt(name account);
    ACTION calctotal(uint64_t startval);

    ACTION payforcpu(name account);

    ACTION testclaim(name from, uint64_t request_id, uint64_t sec_rewind);
    ACTION testupdatecs(name account, uint64_t contribution_score);
    ACTION clearscores();  // DEBUG REMOVE - migrate method
    ACTION migrateplant(uint64_t startval);
    
    ACTION updtotal(); // MIGRATION ACTION
    
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    uint64_t ONE_WEEK = 604800;

    name planted_size = "planted.sz"_n;
    name tx_points_size = "txpt.sz"_n;
    name cs_size = "cs.sz"_n;

    void init_balance(name account);
    void init_harvest_stat(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void _deposit(asset quantity);
    void _withdraw(name account, asset quantity);
    uint32_t calc_transaction_points(name account);
    double get_rep_multiplier(name account);
    void add_planted(name account, asset quantity);
    void sub_planted(name account, asset quantity);
    void change_total(bool add, asset quantity);
    void calc_contribution_score(name account);

    void size_change(name id, int delta);
    void size_set(name id, uint64_t newsize);
    uint64_t get_size(name id);

    // Contract Tables

    // Migration plan - leave this data in there, but start using the planted table
    TABLE balance_table {
      name account;
      asset planted;
      asset reward; // harvest reward - unused

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

    TABLE planted_table {
      name account;
      asset planted;
      uint64_t rank;  

      uint64_t primary_key()const { return account.value; }
      uint128_t by_planted() const { return (uint128_t(planted.amount) << 64) + account.value; } 
      uint64_t by_rank() const { return rank; } 

    };

    typedef eosio::multi_index<"planted"_n, planted_table,
      indexed_by<"byplanted"_n,const_mem_fun<planted_table, uint128_t, &planted_table::by_planted>>,
      indexed_by<"byrank"_n,const_mem_fun<planted_table, uint64_t, &planted_table::by_rank>>
    > planted_tables;

    TABLE tx_points_table {
      name account;
      uint32_t points;
      uint64_t rank;  

      uint64_t primary_key() const { return account.value; } 
      uint64_t by_points() const { return (uint64_t(points) << 32) +  ( (account.value <<32) >> 32) ; } 
      uint64_t by_rank() const { return rank; } 

    };

    typedef eosio::multi_index<"txpoints"_n, tx_points_table,
      indexed_by<"bypoints"_n,const_mem_fun<tx_points_table, uint64_t, &tx_points_table::by_points>>,
      indexed_by<"byrank"_n,const_mem_fun<tx_points_table, uint64_t, &tx_points_table::by_rank>>
    > tx_points_tables;

    DEFINE_CS_POINTS_TABLE

    DEFINE_CS_POINTS_TABLE_MULTI_INDEX

    DEFINE_SIZE_TABLE

    DEFINE_SIZE_TABLE_MULTI_INDEX

    // DEPRECATED - REMOVE ONCE APPS ARE UPDATED // 
    DEFINE_HARVEST_TABLE
    
    // External Tables

    DEFINE_REP_TABLE

    DEFINE_REP_TABLE_MULTI_INDEX

    DEFINE_USER_TABLE

    DEFINE_USER_TABLE_MULTI_INDEX

    DEFINE_CONFIG_TABLE

    DEFINE_CONFIG_TABLE_MULTI_INDEX

    DEFINE_CBS_TABLE

    DEFINE_CBS_TABLE_MULTI_INDEX


    TABLE total_table {
      uint64_t id;
      asset total_planted;
      uint64_t primary_key()const { return id; }
    };
    
    typedef singleton<"total"_n, total_table> total_tables;
    typedef eosio::multi_index<"total"_n, total_table> dump_for_total;

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

    typedef eosio::multi_index<"refunds"_n, refund_table> refund_tables;

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
    planted_tables planted;
    tx_points_tables txpoints;
    cs_points_tables cspoints;
    size_tables sizes;
    total_tables total;

    // DEPRECATED - remove
    typedef eosio::multi_index<"harvest"_n, harvest_table> harvest_tables;
    harvest_tables harveststat;


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
          (unplant)(claimrefund)(cancelrefund)(sow)
          (ranktx)(calctrxpt)(calctrxpts)(rankplanted)(rankplanteds)(calccss)(calccs)(rankcss)(rankcs)(ranktxs)(updatecs)
          (updatetxpt)(clearscores)(migrateplant)(updtotal)(calctotal)
          (testclaim)(testupdatecs))
      }
  }
}
