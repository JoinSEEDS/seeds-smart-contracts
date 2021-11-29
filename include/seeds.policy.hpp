#include <eosio/eosio.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT policy : public contract {
  public:
    using contract::contract;
    policy(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        devicepolicy(receiver, receiver.value),
        expiry(receiver, receiver.value)
      {}

    ACTION reset();

    ACTION create(name account, string backend_user_id, string device_id, string signature, string policy);
    
    ACTION createexp(name account, string backend_user_id, string device_id, string signature, string policy, uint64_t expiry_seconds);

    ACTION update(uint64_t id, name account, string backend_user_id, string device_id, string signature, string policy);

    ACTION remove(uint64_t id);

    ACTION removeexp(uint64_t id);

  private:

    void remove_aux(uint64_t id);

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

    TABLE expiry_table {
      uint64_t id;
      uint64_t created_at;
      uint64_t valid_until;

      uint64_t primary_key()const { return id; }
    };

    typedef eosio::multi_index<"devicepolicy"_n, device_policy_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<device_policy_table, uint64_t, &device_policy_table::by_account>>
    > device_policy_tables;

    typedef eosio::multi_index<"expiry"_n, expiry_table> expiry_tables;

    device_policy_tables devicepolicy;
    expiry_tables expiry;


};

EOSIO_DISPATCH(policy, (create)(createexp)(update)(reset)(remove)(removeexp));
