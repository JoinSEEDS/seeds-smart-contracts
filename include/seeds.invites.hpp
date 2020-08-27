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
      : contract(receiver, code, ds)
        {}

    ACTION send(name from, name to, asset quantity, string memo);

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<invites>(name(receiver), name(code), &invites::send);
  } 
}
