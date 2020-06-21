#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

#include <contracts.hpp>
#include <tables.hpp>
#include <utils.hpp>

using namespace eosio;
using std::string;

CONTRACT history : public contract {

    public:
        using contract::contract;
        history(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          users(contracts::accounts, contracts::accounts.value),
          residents(receiver, receiver.value),
          citizens(receiver, receiver.value)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

        ACTION trxentry(name from, name to, asset quantity);
        
        ACTION clearoldtrx(name account);

        ACTION addcitizen(name account);
        
        ACTION addresident(name account);
    private:
      void check_user(name account);
    
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

    TABLE config_table {
        name param;
        uint64_t value;
        uint64_t primary_key() const { return param.value; }
    };
    
    typedef eosio::multi_index <"config"_n, config_table> config_tables;

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
    
    typedef eosio::multi_index<"users"_n, tables::user_table,
      indexed_by<"byreputation"_n,
      const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
    > user_tables;

    user_tables users;
    resident_tables residents;
    citizen_tables citizens;
};

EOSIO_DISPATCH(history, (reset)(historyentry)(trxentry)(clearoldtrx)(addcitizen)(addresident));
