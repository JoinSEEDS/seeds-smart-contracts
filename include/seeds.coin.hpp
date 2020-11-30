#include <eosio/eosio.hpp>

using namespace eosio;

CONTRACT coin : public contract {
  public:
    using contract::contract;
    coin(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds)
      {}

    ACTION deposit(name from, name to, asset amount, std::string memo);

    ACTION withdraw();

    ACTION freeze(name user);

    ACTION unfreeze(name user);

  private:

    TABLE balance_table {
      name account;
      asset balance;
      uint64_t primary_key()const { return account.value; }
    };

    typedef eosio::multi_index<"balances"_n, balance_table> balance_tables;

    balance_tables balances;


};

EOSIO_DISPATCH(policy, ());
