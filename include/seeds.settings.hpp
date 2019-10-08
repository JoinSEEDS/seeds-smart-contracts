#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;
using std::string;

CONTRACT settings : public contract {
  public:
      using contract::contract;
      settings(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          config(receiver, receiver.value)
          {}

      ACTION reset();

      ACTION configure(name param, uint64_t value);
  private:
      TABLE config_table {
        name param;
        uint64_t value;
        uint64_t primary_key()const { return param.value; }
      };

      typedef eosio::multi_index<"config"_n, config_table> config_tables;

      config_tables config;
};

EOSIO_DISPATCH(settings, (reset)(configure));
