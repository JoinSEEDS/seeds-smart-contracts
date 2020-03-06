#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <tables.hpp>

using namespace eosio;
using std::string;

CONTRACT policy : public contract {
  public:
    using contract::contract;
    policy(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        devicepolicy(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value)
      {}

    ACTION reset();

    ACTION create(name account, string backend_user_id, string device_id, string signature, string policy);

    ACTION update(name account, string backend_user_id, string device_id, string signature, string policy);

  private:
    TABLE policy_table {
      name account;
      string uuid;
      string signature;
      string policy;
      uint64_t primary_key()const { return account.value; }
    };

    TABLE policy_table_new {
      name account;
      string backend_user_id;
      string device_id;
      string signature;
      string policy;
      uint64_t primary_key()const { return account.value; }
    };

    TABLE device_policy_table {
      uint64_t id;
      name account;
      string backend_user_id;
      string device_id;
      string signature;
      string policy;
      uint64_t primary_key()const { return id; }
      uint64_t by_account()const { return account.value; }
    };

    // needed for reset
    typedef eosio::multi_index<"users"_n, tables::user_table,
      indexed_by<"byreputation"_n,
      const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
    > user_tables;


    typedef eosio::multi_index<"policies"_n, policy_table> policy_tables;

    typedef eosio::multi_index<"policiesnew"_n, policy_table_new> policy_tables_new;

    typedef eosio::multi_index<"devicepolicy"_n, device_policy_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<device_policy_table, uint64_t, &device_policy_table::by_account>>
    > device_policy_tables;

    user_tables users;
    device_policy_tables devicepolicy;


};

EOSIO_DISPATCH(policy, (create)(update)(reset));
