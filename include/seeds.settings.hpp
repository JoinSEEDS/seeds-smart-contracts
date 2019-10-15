#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include "seeds.settings.types.hpp"

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

      config_tables config;
};

EOSIO_DISPATCH(settings, (reset)(configure));
