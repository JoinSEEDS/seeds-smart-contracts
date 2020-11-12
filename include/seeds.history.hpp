#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <tables/config_table.hpp>
#include <eosio/singleton.hpp>

#include <contracts.hpp>
#include <tables/user_table.hpp>

#include <iterator>

using namespace eosio;
using std::string;

CONTRACT history : public contract {

    public:
        using contract::contract;
        history(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          users(contracts::accounts, contracts::accounts.value),
          residents(receiver, receiver.value),
          citizens(receiver, receiver.value),
          reputables(receiver, receiver.value),
          regens(receiver, receiver.value),
          qevs(receiver, receiver.value),
          total(contracts::harvest, contracts::harvest.value),
          circulating(contracts::token, contracts::token.value)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

        ACTION trxentry(name from, name to, asset quantity);
        
        ACTION addcitizen(name account);
        
        ACTION addresident(name account);

        ACTION numtrx(name account);

        ACTION addreputable(name organization);

        ACTION addregen(name organization);

        ACTION orgtxpoints(name organization);

        ACTION orgtxpt(name organization, uint128_t start_val, uint64_t chunksize, uint64_t running_total);

        ACTION calcmqev ();

        ACTION calcqev(uint64_t start, uint64_t chunksize, uint64_t volume);

        ACTION testqev();


    private:
      void check_user(name account);
      uint32_t num_transactions(name account, uint32_t limit);
      uint64_t config_get(name key);
      void fire_orgtx_calc(name organization, uint128_t start_val, uint64_t chunksize, uint64_t running_total);
      bool clean_old_tx(name org, uint64_t chunksize);
      uint64_t get_beginning_of_day_in_seconds();
      uint64_t count_transaction_volume_user (name user, uint64_t & max_transactions, uint64_t & transactions_cap, uint64_t * volume);
      uint64_t count_transaction_volume_org (name org, uint64_t & max_transactions, uint64_t & transactions_cap, uint64_t * volume);

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
      
      TABLE transaction_table {
        uint64_t id;
        name to;
        asset quantity;
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp; }
        uint64_t by_to() const { return to.value; }
        uint64_t by_quantity() const { return quantity.amount; }
        uint128_t by_to_quantity() const { return (uint128_t(to.value) << 64) + quantity.amount; }
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
        uint128_t by_to_quantity() const { 
          if (!in) {
            return (uint128_t(other.value) << 64) + quantity.amount;
          } else {
            return uint128_t(0);
          }
        }
      };

      TABLE qev_table {
        uint64_t begin_of_day_timestamp;
        asset qev;
        asset circulating_supply;
        asset planted;
        asset burned; // ?

        uint64_t primary_key() const { return begin_of_day_timestamp; }
      };

      // from harvest
      TABLE total_table {
        uint64_t id;
        asset total_planted;
        uint64_t primary_key()const { return id; }
      };
    
      typedef singleton<"total"_n, total_table> total_tables;
      typedef eosio::multi_index<"total"_n, total_table> dump_for_total;

      TABLE circulating_supply_table {
        uint64_t id;
        uint64_t total;
        uint64_t circulating;
        uint64_t primary_key()const { return id; }
      };

      typedef singleton<"circulating"_n, circulating_supply_table> circulating_supply_tables;
      typedef eosio::multi_index<"circulating"_n, circulating_supply_table> dump_for_circulating;

      typedef eosio::multi_index<"orgtx"_n, org_tx_table,
        indexed_by<"bytimestamp"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_timestamp>>,
        indexed_by<"byquantity"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_quantity>>,
        indexed_by<"byother"_n,const_mem_fun<org_tx_table, uint128_t, &org_tx_table::by_other>>,
        indexed_by<"bytoquantity"_n,const_mem_fun<org_tx_table, uint128_t, &org_tx_table::by_to_quantity>>
      > org_tx_tables;

      typedef eosio::multi_index<"transactions"_n, transaction_table,
        indexed_by<"bytimestamp"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_timestamp>>,
        indexed_by<"byquantity"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_quantity>>,
        indexed_by<"byto"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_to>>,
        indexed_by<"bytoquantity"_n,const_mem_fun<transaction_table, uint128_t, &transaction_table::by_to_quantity>>
      > transaction_tables;

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

      typedef eosio::multi_index<"qevs"_n, qev_table> qev_tables;
      
      DEFINE_USER_TABLE
      
      DEFINE_USER_TABLE_MULTI_INDEX

      user_tables users;
      resident_tables residents;
      citizen_tables citizens;
      reputable_tables reputables;
      regenerative_tables regens;
      qev_tables qevs;
      total_tables total;
      circulating_supply_tables circulating;
};

EOSIO_DISPATCH(history, 
  (reset)
  (historyentry)(trxentry)
  (addcitizen)(addresident)
  (addreputable)(addregen)
  (numtrx)
  (orgtxpoints)(orgtxpt)
  (calcmqev)(calcqev)
  (testqev)
  );
