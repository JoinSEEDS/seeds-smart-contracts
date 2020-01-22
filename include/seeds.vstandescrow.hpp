#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT vstandescrow : public contract {
    public:
        using contract::contract;
        vstandescrow(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
        escrows(receiver, receiver.value),
        sponsors(receiver, receiver.value)
        {}

        ACTION reset();

        ACTION create(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date);

        ACTION createescrow(name sponsor_bucket, name beneficiary_org, asset quantity, uint64_t vesting_date);

        ACTION cancel(name sponsor, name beneficiary);

        ACTION cancelescrow(name sponsor, name beneficiary);

        ACTION claim(name beneficiary);

        ACTION withdraw(name sponsor, asset quantity);

        void ontransfer(name from, name to, asset quantity, string memo);


    private:
        symbol seeds_symbol = symbol("SEEDS", 4);
        enum TypeCreate:uint8_t { createNormal = 0, createEscrow = 1 };

        TABLE escrow_table {
            uint64_t id;
            name sponsor;
            name beneficiary;
            asset quantity;
            uint64_t vesting_date;
            uint8_t type;

            uint64_t primary_key() const { return id; }
            uint64_t by_sponsor() const { return sponsor.value; }
            uint64_t by_beneficiary() const { return beneficiary.value; }
        };

        TABLE sponsors_table {
            name sponsor;
            asset balance;

            uint64_t primary_key() const { return sponsor.value; }
        };


        typedef eosio::multi_index<"escrow"_n, escrow_table,
            indexed_by<"bysponsor"_n, const_mem_fun<escrow_table, uint64_t, &escrow_table::by_sponsor>>,
            indexed_by<"bybneficiary"_n, const_mem_fun<escrow_table, uint64_t, &escrow_table::by_beneficiary>>
        > escrow_tables;

        typedef eosio::multi_index<"sponsors"_n, sponsors_table> sponsors_tables;

        escrow_tables escrows;
        sponsors_tables sponsors;

        void createaux(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date, uint8_t type);
        void cancelaux(name sponsor, name beneficiary);
        void check_asset(asset quantity);
        void init_balance(name user);
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<vstandescrow>(name(receiver), name(code), &vstandescrow::ontransfer);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(vstandescrow, (reset)(create)(createescrow)(cancel)(cancelescrow)(claim)(withdraw))
      }
  }
}

