#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/config_float_table.hpp>

using namespace eosio;
using std::string;

CONTRACT settings : public contract {
  public:
      using contract::contract;
      settings(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          config(receiver, receiver.value),
          configfloat(receiver, receiver.value),
          contracts(receiver, receiver.value)
          {}

      ACTION reset();

      ACTION confwithdesc(name param, uint64_t value, string description, name impact);

      ACTION configure(name param, uint64_t value);

      ACTION conffloat(name param, double value);

      ACTION conffloatdsc(name param, double value, string description, name impact);

      ACTION setcontract(name contract, name account);

      ACTION remove(name param);

  private:
      const name high_impact = "high"_n;
      const name medium_impact = "med"_n;
      const name low_impact = "low"_n;

      DEFINE_CONFIG_TABLE
        
      DEFINE_CONFIG_TABLE_MULTI_INDEX

      DEFINE_CONFIG_FLOAT_TABLE

      DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

      config_tables config;
      config_float_tables configfloat;

      /*
      * Information for clients as to where to find our contracts
      * 
      * Settings contract itself is in this table as well - when settings is migrated the old location
      * will be updated the new account. ("settings")
      * 
      * E.g.
      * contract - the internal name, e.g. "token"
      * account - the Telos account, e.g. "seedstokennx"
      */
      TABLE contracts_table {
        name contract;
        name account;
        uint64_t primary_key()const { return contract.value; }
      };

      typedef eosio::multi_index<"contracts"_n, contracts_table> contracts_tables;

      contracts_tables contracts;

};

EOSIO_DISPATCH(settings, (reset)(configure)(setcontract)(confwithdesc)(conffloat)(conffloatdsc)(remove));
