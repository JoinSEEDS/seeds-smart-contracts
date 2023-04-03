#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <tables.hpp>
#include <tables/price_history_table.hpp>
#include <cmath>

using namespace eosio;
using std::string;

CONTRACT sale : public contract {
  public:
    using contract::contract;
    sale(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        config(receiver, receiver.value),
        sold(receiver, receiver.value),
        price(receiver, receiver.value),
        pricehistory(receiver, receiver.value),
        rounds(receiver, receiver.value),
        dailystats(receiver, receiver.value),
        payhistory(receiver, receiver.value),
        flags(receiver, receiver.value),
        whitelist(receiver, receiver.value)
        {}
      
    ACTION onperiod();
    
    ACTION ontransfer(name buyer, name contract, asset tlos_quantity, string memo);

    ACTION onhusd(name from, name to, asset quantity, string memo);
    
    ACTION newpayment(name recipientAccount, string paymentSymbol, string paymentQuantity, string paymentId, uint64_t multipliedUsdValue);

    ACTION updatetlos(asset tlos_usd);
    
    ACTION updatelimit(asset citizen_limit, asset resident_limit, asset visitor_limit);

    ACTION addround(uint64_t volume, asset token_per_usd);

    ACTION initrounds(uint64_t volume_per_round, asset initial_token_per_usd, asset linear_increment, uint64_t num_rounds);

    ACTION initsale();

    ACTION incprice();

    ACTION priceupdate();

    ACTION pause();

    ACTION unpause();

    ACTION setflag(name flagname, uint64_t value);

    ACTION reset();

    ACTION updatevol(uint64_t round_id, uint64_t volume);

    ACTION addwhitelist(name account);

    ACTION remwhitelist(name account);

    //ACTION testhusd(name from, name to, asset quantity);

  private:

    void purchase_usd(name buyer, asset usd_quantity, string paymentSymbol, string memo); 
    void on_husd(name from, name to, asset quantity, string memo);

    asset token_for_usd(asset usd_quantity, asset token_asset);
    void update_price(); 
    void price_update_aux();
    bool is_paused();
    bool is_set(name flag);
    bool is_whitelisted(name account);
    bool is_less_than_limit(asset hypha_quantity);
    uint64_t get_limit();

    void price_history_update(); 

    symbol tlos_symbol = symbol("TLOS", 4);
    symbol husd_symbol = symbol("HUSD", 2);
    symbol seeds_symbol = symbol("SEEDS", 4);
    symbol hypha_symbol = symbol("HYPHA", 2);
    symbol usd_symbol = symbol("USD", 4);
    symbol usd_symbol_2 = symbol("USD", 2);
    name paused_flag = "paused"_n;
    name tlos_paused_flag = "tlos.paused"_n;
    name whitelist_limit_flag = "whtlst.limit"_n;

    name husd_contract = "husd.hypha"_n;
    name hypha_contract = "hypha.hypha"_n;

    uint64_t asset_factor(asset quantity) {
      //return 100;
      return pow(10, quantity.symbol.precision());
    }

    double asset_factor_d(asset quantity) {
      //return 100.0;
      return double(pow(10, quantity.symbol.precision()));
    }

    double asset_token_amount(asset quantity) {
      return quantity.amount / double(pow(10, quantity.symbol.precision()));
    }

    TABLE configtable {
      asset hypha_usd;
      asset tlos_usd;
      asset citizen_limit;
      asset resident_limit;
      asset visitor_limit;
      uint64_t timestamp;
    };

    TABLE payhistory_table {
      uint64_t id;
      name recipientAccount;
      string paymentSymbol;
      string paymentQuantity;
      string paymentId;
      uint64_t multipliedUsdValue;

      uint64_t primary_key()const { return id; }
      uint64_t by_payment_id()const { return std::hash<std::string>{}(paymentId); }
    };

    TABLE round_table {
      uint64_t id;
      uint64_t max_sold;
      asset hypha_usd;

      uint64_t primary_key()const { return id; }
    };
    
    TABLE stattable {
      name buyer_account;
      uint64_t tokens_purchased;
      
      uint64_t primary_key()const { return buyer_account.value; }
    };

    TABLE soldtable {
      uint64_t id;
      uint64_t total_sold;
      uint64_t primary_key()const { return id; }
    };
    
    TABLE price_table {
      uint64_t id;
      uint64_t current_round_id;
      asset hypha_usd;
      uint64_t remaining;

      uint64_t primary_key()const { return id; }
    };

    TABLE price_history_table { 
      uint64_t id; 
      asset hypha_usd; 
      time_point date; 
      
      uint64_t primary_key()const { return id; } 
    }; 

    typedef eosio::multi_index<"pricehistory"_n, price_history_table> price_history_tables;
    
    TABLE flags_table { 
        name param; 
        uint64_t value; 
        uint64_t primary_key()const { return param.value; } 
      }; 

    typedef eosio::multi_index<"flags"_n, flags_table> flags_tables; 

    TABLE whitelist_table { 
        name account; 
        uint64_t value; 
        uint64_t primary_key()const { return account.value; } 
      }; 
    typedef eosio::multi_index<"whitelist"_n, whitelist_table> whitelist_tables; 

    typedef singleton<"config"_n, configtable> configtables;
    typedef eosio::multi_index<"config"_n, configtable> dump_for_config;

    typedef singleton<"sold"_n, soldtable> soldtables;
    typedef eosio::multi_index<"sold"_n, soldtable> dump_for_sold;

    typedef singleton<"price"_n, price_table> price_tables;
    typedef eosio::multi_index<"price"_n, price_table> dump_for_price;
    
    typedef multi_index<"dailystats"_n, stattable> stattables;
    
    typedef multi_index<"rounds"_n, round_table> round_tables;

    typedef eosio::multi_index<"payhistory"_n, payhistory_table,
      indexed_by<"bypaymentid"_n,const_mem_fun<payhistory_table, uint64_t, &payhistory_table::by_payment_id>>
    > payhistory_tables;

    configtables config;

    soldtables sold;

    price_tables price;

    price_history_tables pricehistory;

    round_tables rounds;

    stattables dailystats;

    payhistory_tables payhistory;

    flags_tables flags;

    whitelist_tables whitelist;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::tlostoken.value) {
      execute_action<sale>(name(receiver), name(code), &sale::ontransfer);
  } else if (action == name("transfer").value && code == "husd.hypha"_n.value) {
      execute_action<sale>(name(receiver), name(code), &sale::onhusd);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(sale, 
          (reset)(onperiod)(updatetlos)(updatelimit)(newpayment)
          (addround)(initsale)(initrounds)(priceupdate)
          (pause)(unpause)(setflag)
          (incprice)
          (updatevol)
          (addwhitelist)(remwhitelist)
          //(testhusd)
          )
      }
  }
}

