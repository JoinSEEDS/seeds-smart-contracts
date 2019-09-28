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
        ACTION giftuser(name account);
        ACTION receivefund(name organization,asset amount);
    private:

        TABLE org_table{
            name account;
            asset per_user;
            asset balance;
            uint64_t primary_key() const { return account.value; }
        };

        multi_index<"orgnization"_n,org_table> org_tables;

        TABLE referral_table{
            uint64_t referral_id;
            name org;
            name user;
            bool citizen_bonus;
            bool resident_bonus;
            uint64_t primary_key() const { return referral_id; }
        };

        multi_index< "referral"_n, referral_table > referral_tables ;

        org_tables orgtable;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == "seedstoken12"_n.value) {
      execute_action<harvest>(name(receiver), name(code), &organization::recievefund);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(organization, (addorg)(editorg)(giftuser))
      }
  }
}



