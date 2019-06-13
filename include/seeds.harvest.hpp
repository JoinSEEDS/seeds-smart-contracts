#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosio.token.hpp>

using namespace eosio;
using std::string;

CONTRACT harvest : public contract {
  public:
    using contract::contract;
    harvest(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        config(name("seedsettings"), name("settings").value),
        users(name("seedsaccnts3"), name("accounts").value)
        {}

    ACTION reset();

    ACTION onperiod();

    ACTION plant(name from, name to, asset quantity, string memo);

    ACTION unplant(name from, asset quantity);

    ACTION claimreward(name from);
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);

    void init_balance(name account);
    void check_user(name account);
    void check_asset(asset quantity);
    void deposit(asset quantity);
    void withdraw(name account, asset quantity);

    TABLE config_table {
      name param;
      uint64_t value;
      uint64_t primary_key()const { return param.value; }
    };

    TABLE balance_table {
      name account;
      asset planted;
      asset reward;
      uint64_t primary_key()const { return account.value; }
    };

    TABLE user_table {
        name account;
        uint64_t primary_key()const { return account.value; }
    };

    typedef eosio::multi_index<"config"_n, config_table> config_tables;
    typedef eosio::multi_index<"balances"_n, balance_table> balance_tables;
    typedef eosio::multi_index<"users"_n, user_table> user_tables;

    config_tables config;
    balance_tables balances;
    user_tables users;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == "seedstoken12"_n.value) {
      execute_action<harvest>(name(receiver), name(code), &harvest::plant);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(harvest, (reset)(onperiod)(unplant)(claimreward))
      }
  }
}
