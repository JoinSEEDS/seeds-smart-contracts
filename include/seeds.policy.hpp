#include <eosio/eosio.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT policy : public contract {
  public:
    using contract::contract;
    policy(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds)
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

    typedef eosio::multi_index<"policies"_n, policy_table> policy_tables;
    typedef eosio::multi_index<"policiesnew"_n, policy_table_new> policy_tables_new;

};

EOSIO_DISPATCH(policy, (create)(update)(reset));
