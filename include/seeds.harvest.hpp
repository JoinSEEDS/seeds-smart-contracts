#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <utils.hpp>

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
        config(contracts::settings, contracts::settings.value),
        users(contracts::accounts, contracts::accounts.value),
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

    ACTION calcplanted();

    ACTION calctrxpt(); // calculate points

    ACTION calctrx(); // calculate score

    ACTION calcrep();

    ACTION calccbs(); // community building score

    ACTION calccs(); // contribution score

    ACTION payforcpu(name account);

    ACTION testreward(name from);
    ACTION testclaim(name from, uint64_t request_id, uint64_t sec_rewind);

    using reset_action = action_wrapper<"reset"_n, &harvest::reset>;
    using unplant_action = action_wrapper<"unplant"_n, &harvest::unplant>;
    using claimreward_action = action_wrapper<"claimreward"_n, &harvest::claimreward>;

  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    uint64_t ONE_WEEK = 604800;

    void init_balance(name account);
    void init_harvest_stat(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void deposit(asset quantity);
    void withdraw(name account, asset quantity);
    void calc_tx_points(name account, uint64_t cycle);

    double get_rep_multiplier(name account) {
        auto hitr = harveststat.find(account.value);
        check(hitr != harveststat.end(), "not a seeds user "+account.to_string());
        return rep_multiplier_for_score(hitr->reputation_score);
    }

    double rep_multiplier_for_score(uint64_t rep_score) {
      // rep is 0 - 100
      check(rep_score < 101, "illegal rep score ");
      // return 0 - 2
      return rep_score * 2.0 / 100.0; 
    }

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

    TABLE harvest_table {
      name account;

      uint64_t planted_score;
      uint64_t planted_timestamp;

      uint64_t transactions_score;
      uint64_t tx_timestamp;

      uint64_t reputation_score;
      uint64_t rep_timestamp;

      uint64_t community_building_score;
      uint64_t community_building_timestamp;
      
      uint64_t contribution_score;
      uint64_t contrib_timestamp;

      uint64_t primary_key()const { return account.value; }
    };

    // External Tables

    TABLE config_table {
      name param;
      uint64_t value;
      uint64_t primary_key()const { return param.value; }
    };

    TABLE user_table {
      name account;
      name status;
      name type;
      string nickname;
      string image;
      string story;
      string roles;
      string skills;
      string interests;
      uint64_t reputation;
      uint64_t timestamp;

      uint64_t primary_key()const { return account.value; }
      uint64_t by_reputation()const { return reputation; }
    };

    // from History contract - scoped by 'from'
    TABLE transaction_table {
       uint64_t id;
       name to;
       asset quantity;
       uint64_t timestamp;

       uint64_t primary_key() const { return id; }
       uint64_t by_timestamp() const { return timestamp; }
       uint64_t by_to() const { return to.value; }
       uint64_t by_quantity() const { return quantity.amount; }
    };

    // from accounts contract
    TABLE cbs_table {
        name account;
        uint64_t community_building_score;

        uint64_t primary_key() const { return account.value; }
        uint64_t by_cbs()const { return community_building_score; }
    };

    typedef eosio::multi_index<"refunds"_n, refund_table> refund_tables;

    typedef eosio::multi_index<"harvest"_n, harvest_table> harvest_tables;

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byplanted"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>>
    > balance_tables;

    typedef eosio::multi_index<"users"_n, user_table,
        indexed_by<"byreputation"_n,
        const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>
    > user_tables;

    typedef eosio::multi_index<"config"_n, config_table> config_tables;

    typedef eosio::multi_index<"transactions"_n, transaction_table,
      indexed_by<"bytimestamp"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_timestamp>>,
      indexed_by<"byquantity"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_quantity>>,
      indexed_by<"byto"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_to>>
    > transaction_tables;

    typedef eosio::multi_index<"cbs"_n, cbs_table,
      indexed_by<"bycbs"_n,
      const_mem_fun<cbs_table, uint64_t, &cbs_table::by_cbs>>
    > cbs_tables;

    // Contract Tables
    balance_tables balances;
    tx_points_tables txpoints;
    harvest_tables harveststat;

    // External Tables
    config_tables config;
    user_tables users;
    cbs_tables cbs;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<harvest>(name(receiver), name(code), &harvest::plant);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(harvest, (payforcpu)(reset)(runharvest)(testreward)(testclaim)(unplant)(claimreward)(claimrefund)(cancelrefund)(sow)(calcrep)(calctrx)(calctrxpt)(calcplanted)(calccbs)(calccs))
      }
  }
}
