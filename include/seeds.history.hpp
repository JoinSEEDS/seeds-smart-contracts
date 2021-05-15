#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <tables/config_table.hpp>
#include <tables/config_float_table.hpp>
#include <tables/size_table.hpp>
#include <tables/organization_table.hpp>

#include <contracts.hpp>
#include <tables/user_table.hpp>

#include <cmath>

using namespace eosio;
using std::string;

CONTRACT history : public contract {

    public:
        using contract::contract;
        history(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          users(contracts::accounts, contracts::accounts.value),
          sizes(receiver, receiver.value),
          residents(receiver, receiver.value),
          citizens(receiver, receiver.value),
          totals(receiver, receiver.value),
          trxcbprewards(receiver, receiver.value),
          organizations(contracts::organization, contracts::organization.value),
          members(contracts::region, contracts::region.value)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

        ACTION trxentry(name from, name to, asset quantity);
        
        ACTION addcitizen(name account);
        
        ACTION addresident(name account);
        
        ACTION numtrx(name account);

        ACTION updatestatus(name account, name scope);

        ACTION deldailytrx(uint64_t day);

        ACTION savepoints(uint64_t id, uint64_t timestamp);

        ACTION sendtrxcbp(name from, name to);

        ACTION testtotalqev(uint64_t numdays, uint64_t volume);
        ACTION migrate();
        ACTION migrateusers();
        ACTION migrateuser(uint64_t start, uint64_t transaction_id, uint64_t chunksize);


    private:
      const uint64_t status_regular = 0;
      const uint64_t status_reputable = 1;
      const uint64_t status_sustainable = 2;
      const uint64_t status_regenerative = 3;
      const uint64_t status_thrivable = 4;

      std::vector<name> status_names = {
        "regular"_n,
        "reputable"_n,
        "sustainable"_n,
        "regenerative"_n,
        "thrivable"_n
      };

      void check_user(name account);
      uint32_t num_transactions(name account, uint32_t limit);
      uint64_t config_get(name key);
      void size_change(name id, int delta);
      void size_set(name id, uint64_t newsize);
      uint64_t get_size(name id);
      void fire_orgtx_calc(name organization, uint128_t start_val, uint64_t chunksize, uint64_t running_total);
      bool clean_old_tx(name org, uint64_t chunksize);
      void save_from_metrics (name from, int64_t & from_points, int64_t & qualifying_volume, uint64_t & day);
      void send_update_txpoints (name from);
      double config_float_get(name key);
      double get_transaction_multiplier(name account, name other);
      void send_trx_cbp_reward_action(name from, name to);
      void send_add_cbs(name account, int points);
      void trx_cbp_reward(name account, name key);
      
      // migration functions
      void save_migration_user_transaction(name from, name to, asset quantity, uint64_t timestamp);
      void adjust_transactions(uint64_t id, uint64_t timestamp);
      uint64_t get_deferred_id();

      TABLE citizen_table {
        uint64_t id;
        name account;
        uint64_t timestamp;
        
        uint64_t primary_key()const { return id; }
        uint64_t by_account()const { return account.value; }
      };
      
      TABLE resident_table {
        uint64_t id;
        name account;
        uint64_t timestamp;
        
        uint64_t primary_key()const { return id; }
        uint64_t by_account()const { return account.value; }
      };

      TABLE history_table {
        uint64_t history_id;
        name account;
        string action;
        uint64_t amount;
        string meta;
        uint64_t timestamp;

        uint64_t primary_key()const { return history_id; }
      };

      TABLE account_status_table {
        uint64_t id;
        name account;
        uint64_t timestamp;

        uint64_t primary_key()const { return id; }
        uint64_t by_account()const { return account.value; }
        uint64_t by_timestamp()const { return timestamp; }
      };
      
      // --------------------------------------------------- //
      // old tables

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

      TABLE org_tx_table {
        uint64_t id;
        name other;
        bool in;
        asset quantity;
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp; }
        uint64_t by_quantity() const { return quantity.amount; }
        uint128_t by_other() const { return (uint128_t(other.value) << 64) + id; }
      };

      typedef eosio::multi_index<"orgtx"_n, org_tx_table,
        indexed_by<"bytimestamp"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_timestamp>>,
        indexed_by<"byquantity"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_quantity>>,
        indexed_by<"byother"_n,const_mem_fun<org_tx_table, uint128_t, &org_tx_table::by_other>>
      > org_tx_tables;

      typedef eosio::multi_index<"transactions"_n, transaction_table,
        indexed_by<"bytimestamp"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_timestamp>>,
        indexed_by<"byquantity"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_quantity>>,
        indexed_by<"byto"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_to>>
      > transaction_tables;

      // --------------------------------------------------- //

      TABLE daily_transactions_table { // scoped by beginning_of_day_in_seconds
        uint64_t id;
        name from;
        name to;
        uint64_t volume;
        uint64_t qualifying_volume;
        uint64_t from_points;
        uint64_t to_points;
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_from() const { return from.value; }
        uint64_t by_to() const { return to.value; }
        uint64_t by_timestamp() const { return timestamp; }
        uint128_t by_from_to() const { return (uint128_t(from.value) << 64) + to.value; }
      };

      TABLE transaction_points_table { // scoped by account
        uint64_t timestamp;
        uint64_t points;

        uint64_t primary_key() const { return timestamp; }
        uint64_t by_points() const { return points; }
      };

      TABLE qev_table { // scoped by account
        uint64_t timestamp;
        uint64_t qualifying_volume;

        uint64_t primary_key() const { return timestamp; }
        uint64_t by_volume() const { return qualifying_volume; }
      };

      TABLE totals_table {
        name account;
        uint64_t total_volume;
        uint64_t total_number_of_transactions;
        uint64_t total_incoming_from_rep_orgs;
        uint64_t total_outgoing_to_rep_orgs;

        uint64_t primary_key() const { return account.value; }
      };

      DEFINE_ORGANIZATION_TABLE

      DEFINE_ORGANIZATION_TABLE_MULTI_INDEX

      TABLE members_table {
          name region;
          name account;
          time_point joined_date = current_block_time().to_time_point();

          uint64_t primary_key() const { return account.value; }
          uint64_t by_region() const { return region.value; }
      };

      TABLE trx_cbp_rewards_table {
        uint64_t id;
        name account;
        name key;
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint128_t by_account_key() const { return (uint128_t(account.value) << 64) + key.value; }
      };

      TABLE deferred_id_table {
        uint64_t id;
      };

      typedef singleton<"deferredids"_n, deferred_id_table> deferred_id_tables;
      typedef eosio::multi_index<"deferredids"_n, deferred_id_table> dump_for_deferred_id;

      typedef eosio::multi_index<"citizens"_n, citizen_table,
        indexed_by<"byaccount"_n,
        const_mem_fun<citizen_table, uint64_t, &citizen_table::by_account>>
      > citizen_tables;
      
      typedef eosio::multi_index<"residents"_n, resident_table, 
        indexed_by<"byaccount"_n,
        const_mem_fun<resident_table, uint64_t, &resident_table::by_account>>
      > resident_tables;
      
      typedef eosio::multi_index<"history"_n, history_table> history_tables;

      typedef eosio::multi_index<"acctstatus"_n, account_status_table,
        indexed_by<"byaccount"_n,
        const_mem_fun<account_status_table, uint64_t, &account_status_table::by_account>>,
        indexed_by<"bytimestamp"_n,
        const_mem_fun<account_status_table, uint64_t, &account_status_table::by_timestamp>>
      > account_status_tables;

      typedef eosio::multi_index<"dailytrxs"_n, daily_transactions_table,
        indexed_by<"byfrom"_n,
        const_mem_fun<daily_transactions_table, uint64_t, &daily_transactions_table::by_from>>,
        indexed_by<"byto"_n,
        const_mem_fun<daily_transactions_table, uint64_t, &daily_transactions_table::by_to>>,
        indexed_by<"bytimestamp"_n,
        const_mem_fun<daily_transactions_table, uint64_t, &daily_transactions_table::by_timestamp>>,
        indexed_by<"byfromto"_n,
        const_mem_fun<daily_transactions_table, uint128_t, &daily_transactions_table::by_from_to>>
      > daily_transactions_tables;

      typedef eosio::multi_index<"trxpoints"_n, transaction_points_table,
        indexed_by<"bypoints"_n,
        const_mem_fun<transaction_points_table, uint64_t, &transaction_points_table::by_points>>
      > transaction_points_tables;

      typedef eosio::multi_index<"qevs"_n, qev_table,
        indexed_by<"byvolume"_n,
        const_mem_fun<qev_table, uint64_t, &qev_table::by_volume>>
      > qev_tables;

      typedef eosio::multi_index<"totals"_n, totals_table> totals_tables;

      typedef eosio::multi_index <"members"_n, members_table,
        indexed_by<"byregion"_n,const_mem_fun<members_table, uint64_t, &members_table::by_region>>
      > members_tables;

      typedef eosio::multi_index<"trxcbpreward"_n, trx_cbp_rewards_table,
        indexed_by<"byacctkey"_n,
        const_mem_fun<trx_cbp_rewards_table, uint128_t, &trx_cbp_rewards_table::by_account_key>>
      > trx_cbp_rewards_tables;

      DEFINE_USER_TABLE
      
      DEFINE_USER_TABLE_MULTI_INDEX

      DEFINE_SIZE_TABLE

      DEFINE_SIZE_TABLE_MULTI_INDEX

      user_tables users;
      resident_tables residents;
      citizen_tables citizens;
      totals_tables totals;
      size_tables sizes;
      organization_tables organizations;
      members_tables members;
      trx_cbp_rewards_tables trxcbprewards;
};

EOSIO_DISPATCH(history, 
  (reset)
  (historyentry)(trxentry)
  (addcitizen)(addresident)
  (updatestatus)
  (numtrx)
  (deldailytrx)(savepoints)
  (testtotalqev)
  (sendtrxcbp)
  (migrateusers)(migrateuser)
  (migrate)
);
