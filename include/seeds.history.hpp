#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>

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
          reputables(receiver, receiver.value),
          regens(receiver, receiver.value),
          totals(receiver, receiver.value)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

        ACTION trxentry(name from, name to, asset quantity);
        
        ACTION addcitizen(name account);
        
        ACTION addresident(name account);
        
        ACTION numtrx(name account);

        ACTION addreputable(name organization);

        ACTION addregen(name organization);

        ACTION deldailytrx(uint64_t day);

        ACTION savepoints(uint64_t id, uint64_t timestamp);

        ACTION testtotalqev(uint64_t numdays, uint64_t volume);
        ACTION migrate();
        ACTION migrateusers();
        ACTION migrateuser(uint64_t start, uint64_t transaction_id, uint64_t chunksize);


    private:
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
      
      // migration functions
      void save_migration_user_transaction(name from, name to, asset quantity, uint64_t timestamp);
      void adjust_transactions(uint64_t id, uint64_t timestamp);

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

      TABLE reputable_table {
        uint64_t id;
        name organization;
        uint64_t timestamp;

        uint64_t primary_key()const { return id; }
        uint64_t by_org()const { return organization.value; }
      };

      TABLE regenerative_table {
        uint64_t id;
        name organization;
        uint64_t timestamp;

        uint64_t primary_key()const { return id; }
        uint64_t by_org()const { return organization.value; }
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

      typedef eosio::multi_index<"citizens"_n, citizen_table,
        indexed_by<"byaccount"_n,
        const_mem_fun<citizen_table, uint64_t, &citizen_table::by_account>>
      > citizen_tables;
      
      typedef eosio::multi_index<"residents"_n, resident_table, 
        indexed_by<"byaccount"_n,
        const_mem_fun<resident_table, uint64_t, &resident_table::by_account>>
      > resident_tables;
      
      typedef eosio::multi_index<"history"_n, history_table> history_tables;

      typedef eosio::multi_index<"reputables"_n, reputable_table,
        indexed_by<"byorg"_n,
        const_mem_fun<reputable_table, uint64_t, &reputable_table::by_org>>
      > reputable_tables;

      typedef eosio::multi_index<"regens"_n, regenerative_table,
        indexed_by<"byorg"_n,
        const_mem_fun<regenerative_table, uint64_t, &regenerative_table::by_org>>
      > regenerative_tables;

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
      
      DEFINE_USER_TABLE
      
      DEFINE_USER_TABLE_MULTI_INDEX

      DEFINE_SIZE_TABLE

      DEFINE_SIZE_TABLE_MULTI_INDEX

      user_tables users;
      resident_tables residents;
      citizen_tables citizens;
      reputable_tables reputables;
      regenerative_tables regens;
      totals_tables totals;
      size_tables sizes;
};

EOSIO_DISPATCH(history, 
  (reset)
  (historyentry)(trxentry)
  (addcitizen)(addresident)
  (addreputable)(addregen)
  (numtrx)
  (deldailytrx)(savepoints)
  (testtotalqev)
  (migrateusers)(migrateuser)
  (migrate)
);
