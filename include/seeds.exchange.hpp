#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT exchange : public contract {
  public:
    using contract::contract;
    exchange(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        config(receiver, receiver.value),
        dailystats(receiver, receiver.value)
        {}
      
    ACTION dailyreset();
    
    ACTION purchase(name buyer, name exchange, asset tlos_quantity, string memo);
    
    ACTION updaterate(uint64_t seeds_per_tlos);
    
    ACTION updatelimit(uint64_t seeds_per_day);
  private:
    symbol tlos_symbol = symbol("TLOS", 4);
    symbol seeds_symbol = symbol("SEEDS", 4);
  
    TABLE config_table {
      uint64_t rate;
      uint64_t limit;
      uint64_t rate_timestamp;
    };
    
    TABLE daily_stat_table {
      name buyer_account;
      uint64_t seeds_purchased;
      
      uint64_t primary_key()const { return buyer_account.value; }
    };
    
    typedef singleton<"config"_n, config_table> config_tables;
    
    typedef multi_index<"dailystats"_n, daily_stat_table> daily_stat_tables;
    
    config_tables config;
    
    daily_stat_tables dailystats;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::tlostoken.value) {
      execute_action<exchange>(name(receiver), name(code), &exchange::purchase);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(exchange, (dailyreset)(updaterate)(updatelimit))
      }
  }
}

