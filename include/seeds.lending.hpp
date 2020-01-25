#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;
using std::string;

CONTRACT lending : public contract {
    public:
        using contract::contract;
        lending(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              config(receiver, receiver.value),
              balances(receiver, receiver.value)
              {}

        ACTION borrow(name account, name contract, asset tlos_quantity, string memo);

        ACTION refund(name account, name contract, asset seeds_quantity, string memo);

        ACTION updaterate(asset seeds_per_tlos);

        ACTION updatefee(uint64_t fee_percent);

        ACTION reset();
    private:
        symbol tlos_symbol = symbol("TLOS", 4);
        symbol seeds_symbol = symbol("SEEDS", 4);

        TABLE config_table {
            asset rate;
            uint64_t fee;
        };

        TABLE balance_table {
            name borrower_account;
            asset tlos_deposit;

            uint64_t primary_key()const { return borrower_account.value; }
        };

        typedef singleton<"config"_n, config_table> config_tables;
        typedef eosio::multi_index<"config"_n, config_table> abi_config;

        typedef multi_index<"balances"_n, balance_table> balance_tables;

        config_tables config;

        balance_tables balances;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == name("transfer").value && code == contracts::tlostoken.value) {
        execute_action<lending>(name(receiver), name(code), &lending::borrow);
    } else if (action == name("transfer").value && code == contracts::token.value) {
        execute_action<lending>(name(receiver), name(code), &lending::refund); 
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(lending, (reset)(updaterate)(updatefee))
        }
    }
}