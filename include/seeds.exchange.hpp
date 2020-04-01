#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <tables.hpp>

using namespace eosio;
using std::string;

CONTRACT exchange : public contract {
  public:
    using contract::contract;
    exchange(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        config(receiver, receiver.value),
        sold(receiver, receiver.value),
        dailystats(receiver, receiver.value)
        {}
      
    ACTION onperiod();
    
    ACTION purchase(name buyer, name contract, asset tlos_quantity, string memo);
    
    ACTION updaterate(asset seeds_per_tlos);
    
    ACTION updatelimit(asset citizen_limit, asset resident_limit, asset visitor_limit);

    ACTION reset();

  private:
    symbol tlos_symbol = symbol("TLOS", 4);
    symbol seeds_symbol = symbol("SEEDS", 4);
  
    TABLE configtable {
      asset rate;
      asset citizen_limit;
      asset resident_limit;
      asset visitor_limit;
      uint64_t timestamp;
    };
    
    TABLE stattable {
      name buyer_account;
      uint64_t seeds_purchased;
      
      uint64_t primary_key()const { return buyer_account.value; }
    };

    TABLE soldtable {
      uint64_t id;
      uint64_t total_sold;
      uint64_t primary_key()const { return id; }
    };
    
    typedef singleton<"config"_n, configtable> configtables;
    typedef eosio::multi_index<"config"_n, configtable> dump_for_config;

    typedef singleton<"sold"_n, soldtable> soldtables;
    typedef eosio::multi_index<"sold"_n, soldtable> dump_for_sold;

    typedef multi_index<"dailystats"_n, stattable> stattables;
    
    configtables config;

    soldtables sold;
    
    stattables dailystats;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::tlostoken.value) {
      execute_action<exchange>(name(receiver), name(code), &exchange::purchase);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(exchange, (reset)(onperiod)(updaterate)(updatelimit))
      }
  }
}

