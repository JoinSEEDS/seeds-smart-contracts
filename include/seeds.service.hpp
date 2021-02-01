#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>

using namespace eosio;
using namespace utils;
using std::string;

CONTRACT service : public contract {

  public:

    using contract::contract;
    service(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value)
        {}

    ACTION reset();

    ACTION deposit(name from, name to, asset quantity, string memo);

    ACTION pause(name account);

    ACTION unpause(name account);

    ACTION createinvite(name sponsor, asset transfer_quantity, asset sow_quantity, checksum256 hash);

  private:

    void check_user(name account);
    void init_balance(name account);
    void add_balance(name account, asset quantity);
    void sub_balance (name account, asset quantity);
    void check_paused (name account);
    void _transfer (name beneficiary, asset quantity, string memo);

    DEFINE_USER_TABLE

    DEFINE_USER_TABLE_MULTI_INDEX

    TABLE balance_table {
      name account;
      asset balance;
      bool is_paused;

      uint64_t primary_key() const { return account.value; }
    };

    typedef eosio::multi_index<"balances"_n, balance_table> balance_tables;

    balance_tables balances;
    user_tables users;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<service>(name(receiver), name(code), &service::deposit);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(service, 
          (reset)(deposit)
          (pause)(unpause)
          (createinvite)
        )
      }
  }
}
