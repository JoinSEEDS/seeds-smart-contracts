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
#include <tables/config_float_table.hpp>
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
        monthlyqevs(receiver, receiver.value),
        mintrate(receiver, receiver.value),
        regioncstemp(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value),
        configfloat(contracts::settings, contracts::settings.value),
        users(contracts::accounts, contracts::accounts.value),
        rep(contracts::accounts, contracts::accounts.value),
        cbs(contracts::accounts, contracts::accounts.value),
        circulating(contracts::token, contracts::token.value),
        regions(contracts::region, contracts::region.value),
        members(contracts::region, contracts::region.value)
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
    ACTION rankorgtxs(); // rank org transaction score
    ACTION ranktx(uint64_t start_val, uint64_t chunk, uint64_t chunksize, name table);

    ACTION calccss(); // calculate contribution points // 1h inteval
    ACTION updatecs(name account); 
    ACTION calccs(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

    ACTION rankcss(); // rank contribution score //
    ACTION rankorgcss();
    ACTION rankcs(uint64_t start_val, uint64_t chunk, uint64_t chunksize, name cs_scope);

    ACTION rankrgncss();
    ACTION rankrgncs(uint64_t start, uint64_t chunk, uint64_t chunksize);

    ACTION updatetxpt(name account);
    ACTION calctotal(uint64_t startval);

    ACTION payforcpu(name account);

    ACTION testclaim(name from, uint64_t request_id, uint64_t sec_rewind);
    ACTION testupdatecs(name account, uint64_t contribution_score);
    ACTION testcspoints(name account, uint64_t contribution_points);
    
    ACTION updtotal(); // MIGRATION ACTION

    ACTION setorgtxpt(name organization, uint64_t tx_points);

    ACTION calcmqevs();

    ACTION testcalcmqev(uint64_t day, uint64_t total_volume, uint64_t circulating);
    ACTION calcmintrate();

    ACTION disthvstusrs(uint64_t start, uint64_t chunksize, asset total_amount);
    ACTION disthvstorgs(uint64_t start, uint64_t chunksize, asset total_amount);
    ACTION disthvstrgns(uint64_t start, uint64_t chunksize, asset total_amount);

    ACTION migorgs(uint64_t start);
    ACTION delcsorg(uint64_t start);
    ACTION testmigscope(name account, uint64_t amount);

  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    symbol test_symbol = symbol("TESTS", 4);
    uint64_t ONE_WEEK = 604800;

    name planted_size = "planted.sz"_n;
    name tx_points_size = "txpt.sz"_n;
    name org_tx_points_size = "org.tx.sz"_n;
    name cs_size = "cs.sz"_n;
    name sum_rank_users = "usr.rnk.sz"_n;
    name sum_rank_orgs = "org.rnk.sz"_n;
    name sum_rank_rgns = "rgn.rnk.sz"_n;
    name cs_rgn_size = "rgn.cs.sz"_n;
    name cs_org_size = "org.cs.sz"_n;

    const name individual_scope_accounts = contracts::accounts;
    const name individual_scope_harvest = get_self();
    const name organization_scope = "org"_n;

    const name rgn_status_active = "active"_n;

    void init_balance(name account);
    void init_harvest_stat(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void _deposit(asset quantity);
    void _withdraw(name account, asset quantity);
    uint32_t calc_transaction_points(name account, name type);
    double get_rep_multiplier(name account);
    void add_planted(name account, asset quantity);
    void sub_planted(name account, asset quantity);
    void change_total(bool add, asset quantity);
    void calc_contribution_score(name account, name type);
    void add_cs_to_region(name account, uint32_t points);

    void size_change(name id, int delta);
    void size_set(name id, uint64_t newsize);
    uint64_t get_size(name id);

    uint64_t config_get(name key);
    double config_float_get(name key);
    void send_distribute_harvest (name key, asset amount);
    void withdraw_aux(name sender, name beneficiary, asset quantity, string memo);

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

    DEFINE_CONFIG_FLOAT_TABLE

    DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

    DEFINE_CBS_TABLE

    DEFINE_CBS_TABLE_MULTI_INDEX

    TABLE region_table {
      name id;
      name founder;
      name status; // "active" "inactive"
      string description;
      string locationjson; // json description of the area
      float latitude;
      float longitude;
      uint64_t members_count;
      time_point created_at = current_block_time().to_time_point();

      uint64_t primary_key() const { return id.value; }
      uint64_t by_status() const { return status.value; }
      uint64_t by_count() const { return members_count; }
      uint128_t by_status_id() const { return (uint128_t(status.value) << 64) + id.value; }
    };

    typedef eosio::multi_index <"regions"_n, region_table,
      indexed_by<"bystatus"_n,const_mem_fun<region_table, uint64_t, &region_table::by_status>>,
      indexed_by<"bycount"_n,const_mem_fun<region_table, uint64_t, &region_table::by_count>>,
      indexed_by<"bystatusid"_n,const_mem_fun<region_table, uint128_t, &region_table::by_status_id>>
    > region_tables;


    TABLE total_table {
      uint64_t id;
      asset total_planted;
      uint64_t primary_key()const { return id; }
    };
    
    typedef singleton<"total"_n, total_table> total_tables;
    typedef eosio::multi_index<"total"_n, total_table> dump_for_total;

    typedef eosio::multi_index<"refunds"_n, refund_table> refund_tables;

    typedef eosio::multi_index<"balances"_n, balance_table,
        indexed_by<"byplanted"_n,
        const_mem_fun<balance_table, uint64_t, &balance_table::by_planted>>
    > balance_tables;

    // From history contract
    TABLE transaction_points_table { // scoped by account
      uint64_t timestamp;
      uint64_t points;

      uint64_t primary_key() const { return timestamp; }
      uint64_t by_points() const { return points; }
    };

    // From history contract
    TABLE qev_table { // scoped by account
      uint64_t timestamp;
      uint64_t qualifying_volume;

      uint64_t primary_key() const { return timestamp; }
      uint64_t by_volume() const { return qualifying_volume; }
    };

    TABLE monthly_qev_table {
      uint64_t timestamp;
      uint64_t qualifying_volume;
      uint64_t circulating_supply;

      uint64_t primary_key() const { return timestamp; }
      uint64_t by_volume() const { return qualifying_volume; }
    };

    // From token contract
    TABLE circulating_supply_table {
      uint64_t id;
      uint64_t total;
      uint64_t circulating;

      uint64_t primary_key()const { return id; }
    };

    TABLE region_cs_temporal_table {
      name region;
      uint32_t points;

      uint64_t primary_key()const { return region.value; }
      uint64_t by_cs_points() const { return (uint64_t(points) << 32) +  ( (region.value <<32) >> 32) ; }
    };

    typedef eosio::multi_index<"trxpoints"_n, transaction_points_table,
      indexed_by<"bypoints"_n,
      const_mem_fun<transaction_points_table, uint64_t, &transaction_points_table::by_points>>
    > transaction_points_tables;

    typedef eosio::multi_index<"qevs"_n, qev_table,
      indexed_by<"byvolume"_n,
      const_mem_fun<qev_table, uint64_t, &qev_table::by_volume>>
    > qev_tables;

    typedef eosio::multi_index<"monthlyqevs"_n, monthly_qev_table,
      indexed_by<"byvolume"_n,
      const_mem_fun<monthly_qev_table, uint64_t, &monthly_qev_table::by_volume>>
    > monthly_qev_tables;

    typedef singleton<"circulating"_n, circulating_supply_table> circulating_supply_tables;
    typedef eosio::multi_index<"circulating"_n, circulating_supply_table> dump_for_circulating;

    typedef eosio::multi_index<"regioncstemp"_n, region_cs_temporal_table,
      indexed_by<"bycspoints"_n,
      const_mem_fun<region_cs_temporal_table, uint64_t, &region_cs_temporal_table::by_cs_points>>
    > region_cs_temporal_tables;

    TABLE mint_rate_table {
      uint64_t id;
      int64_t mint_rate;
      int64_t volume_growth;
      uint64_t timestamp;

      uint64_t primary_key() const { return id; }
    };

    typedef eosio::multi_index<"mintrate"_n, mint_rate_table> mint_rate_tables;

    TABLE members_table {
      name region;
      name account;
      time_point joined_date = current_block_time().to_time_point();

      uint64_t primary_key() const { return account.value; }
      uint64_t by_region() const { return region.value; }
    };
    typedef eosio::multi_index <"members"_n, members_table,
        indexed_by<"byregion"_n,const_mem_fun<members_table, uint64_t, &members_table::by_region>>
    > members_tables;



    // Contract Tables
    balance_tables balances;
    planted_tables planted;
    tx_points_tables txpoints;
    cs_points_tables cspoints;
    size_tables sizes;
    monthly_qev_tables monthlyqevs;
    mint_rate_tables mintrate;
    region_cs_temporal_tables regioncstemp;

    // DEPRECATED - remove
    typedef eosio::multi_index<"harvest"_n, harvest_table> harvest_tables;
    harvest_tables harveststat;


    // External Tables
    config_tables config;
    config_float_tables configfloat;
    user_tables users;
    cbs_tables cbs;
    rep_tables rep;
    total_tables total;
    circulating_supply_tables circulating;
    region_tables regions;
    members_tables members;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<harvest>(name(receiver), name(code), &harvest::plant);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(harvest, 
          (payforcpu)(reset)
          (unplant)(claimrefund)(cancelrefund)(sow)
          (ranktx)(calctrxpt)(calctrxpts)(rankplanted)(rankplanteds)(calccss)(calccs)(rankcss)(rankorgcss)(rankcs)(ranktxs)(rankorgtxs)(updatecs)(rankrgncss)(rankrgncs)
          (updatetxpt)(updtotal)(calctotal)
          (setorgtxpt)
          (testclaim)(testupdatecs)(testcalcmqev)(testcspoints)
          (calcmqevs)(calcmintrate)
          (runharvest)(disthvstusrs)(disthvstorgs)(disthvstrgns)
          (delcsorg)(migorgs)(testmigscope)
        )
      }
  }
}
