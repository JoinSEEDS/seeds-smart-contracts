#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <abieos_numeric.hpp>
#include <contracts.hpp>
#include <tables.hpp>
#include <utils.hpp>

using namespace tables;
using namespace eosio;

using std::string;
using std::vector;
using std::make_tuple;
using abieos::keystring_authority;
using abieos::authority;

CONTRACT invites : public contract {
  public:
    using contract::contract;
    invites(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        sponsors(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value)
        {}

    ACTION reset();
    ACTION send(name from, name to, asset quantity, string memo);
    ACTION accept(name sponsor, name account, string publicKey, asset quantity);
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    symbol network_symbol = symbol("TLOS", 4);
    uint64_t sow_amount = 50000;

    void create_account(name account, string publicKey);
    void add_user(name account);
    void transfer_seeds(name account, asset quantity);
    void plant_seeds(asset quantity);
    void sow_seeds(name account, asset quantity);

    TABLE sponsor_table {
      name account;
      asset balance;

      uint64_t primary_key() const { return account.value; }
    };

    typedef multi_index<"sponsors"_n, sponsor_table> sponsor_tables;
    typedef eosio::multi_index<"users"_n, tables::user_table,
      indexed_by<"byreputation"_n,
      const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
    > user_tables;

    sponsor_tables sponsors;
    user_tables users;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<invites>(name(receiver), name(code), &invites::send);
  } else if (code == receiver) {
      switch (action) {
      EOSIO_DISPATCH_HELPER(invites, (reset)(accept))
      }
  }
}
