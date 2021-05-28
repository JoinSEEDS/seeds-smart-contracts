#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>

using namespace eosio;
using std::string;

CONTRACT pool : public contract {

  public:

    using contract::contract;
    pool(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        balances(receiver, receiver.value),
        sizes(receiver, receiver.value),
        config(contracts::settings, contracts::settings.value)
        {}

    ACTION reset();

    ACTION ontransfer(name from, name to, asset quantity, string memo);

    ACTION payouts(asset quantity);

    ACTION payout(asset quantity, uint64_t start, uint64_t chunksize, int64_t old_total_balance);


  private:

    const name total_balance_size = "total.sz"_n;

    void send_transfer(const name & to, const asset & quantity, const string & memo);

    DEFINE_CONFIG_TABLE
    DEFINE_CONFIG_TABLE_MULTI_INDEX
    DEFINE_CONFIG_GET

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX
    DEFINE_SIZE_GET
    DEFINE_SIZE_CHANGE
    DEFINE_SIZE_SET

    TABLE balances_table {
      name account;
      asset balance;

      uint64_t primary_key () const { return account.value; }
    };

    typedef eosio::multi_index<"balances"_n, balances_table> balances_tables;

    balances_tables balances;
    size_tables sizes;

    // external tables
    config_tables config;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<pool>(name(receiver), name(code), &pool::ontransfer);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(pool, 
          (reset)
          (payouts)(payout)
        )
      }
  }
}
