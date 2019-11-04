#include <eosio/eosio.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT history : public contract {

    public:
        using contract::contract;
        history(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds)
        {}

        ACTION reset(name account);

        ACTION historyentry(name account, string action, uint64_t amount, string meta);

    private:

      TABLE history_table {
        uint64_t history_id;
        name account;
        string action;
        uint64_t amount;
        string meta;

        uint64_t primary_key()const { return history_id; }
      };

    typedef eosio::multi_index<"history"_n, history_table> history_tables;

};

EOSIO_DISPATCH(history, (reset)(historyentry));
