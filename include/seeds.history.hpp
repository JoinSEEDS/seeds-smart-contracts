#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

#include <contracts.hpp>
#include <tables/user_table.hpp>

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
          regens(receiver, receiver.value)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

        ACTION trxentry(name from, name to, asset quantity);
        
        ACTION addcitizen(name account);
        
        ACTION addresident(name account);

        ACTION numtrx(name account);

        ACTION addreputable(name organization);

        ACTION addregen(name organization);

    private:
      void check_user(name account);
      uint32_t num_transactions(name account, uint32_t limit);

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

      typedef eosio::multi_index<"transactions"_n, transaction_table,
        indexed_by<"bytimestamp"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_timestamp>>,
        indexed_by<"byquantity"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_quantity>>,
        indexed_by<"byto"_n,const_mem_fun<transaction_table, uint64_t, &transaction_table::by_to>>
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
      
      DEFINE_USER_TABLE
      
      DEFINE_USER_TABLE_MULTI_INDEX

      user_tables users;
      resident_tables residents;
      citizen_tables citizens;
      reputable_tables reputables;
      regenerative_tables regens;
};

EOSIO_DISPATCH(history, (reset)(historyentry)(trxentry)(addcitizen)(addresident)(numtrx)(addreputable)(addregen));
