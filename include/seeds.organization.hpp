#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std;

CONTRACT organization : public contract{
    public:
        using contract::contract;
        organization(name receiver,name code, datastream<const char *> ds)
        : contract(receiver,code,ds),
         orgtable(receiver,receiver.value)
        {}
        
        ACTION addorg(name organization,asset acctseed);
        ACTION editorg(name organization,asset acctseed);
        ACTION giftuser(name organization,name user);
        ACTION receivefund(name from,name to,asset amount,string memo);
    private:

        TABLE org_table{
            name account;
            asset per_user;
            asset balance;
            uint64_t primary_key() const { return account.value; }
        };

        typedef multi_index<"orgnization"_n,org_table> org_tables;

    void depositfunds(account_name to, asset balance)
    {   action(
                permission_level{ _self, "active"_n },
                "seedstoken12_n, "transfer"_n,
                std::make_tuple(_self,,ref_commission, std::string("Joining Bonus From Seeds"))
        ).send();

    }

    org_tables orgtable;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == "seedstoken12"_n.value) {
      execute_action<organization>(name(receiver), name(code), &organization::receivefund);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(organization, (addorg)(editorg)(giftuser))
      }
  }
}



