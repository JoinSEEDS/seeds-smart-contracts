#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <tables/user_table.hpp>

using namespace eosio;
using std::string;

CONTRACT policy : public contract {
  public:
    using contract::contract;
    policy(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        devicepolicy(receiver, receiver.value)
      {}

    ACTION reset();

    ACTION create(name account, string backend_user_id, string device_id, string signature, string policy);

    ACTION update(uint64_t id, name account, string backend_user_id, string device_id, string signature, string policy);

    ACTION remove(uint64_t id);

  private:
    void check_user(name account);

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

    typedef eosio::multi_index<"devicepolicy"_n, device_policy_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<device_policy_table, uint64_t, &device_policy_table::by_account>>
    > device_policy_tables;

    device_policy_tables devicepolicy;

    
};

EOSIO_DISPATCH(policy, (create)(update)(reset)(remove));
