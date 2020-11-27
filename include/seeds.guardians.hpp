#include <eosio/eosio.hpp>

using namespace eosio;

CONTRACT guardians : public contract {
    public:
        using contract::contract;
        guardians(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds)
            {}

        ACTION reset();
    private:

}

EOSIO_DISPATCH(guardians, (reset));