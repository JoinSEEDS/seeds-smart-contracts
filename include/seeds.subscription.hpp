#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <seeds.token.hpp>
#include "seeds.settings.types.hpp"
#include "seeds.accounts.types.hpp"

using namespace eosio;
using std::string;

CONTRACT subscription : public contract {
  public:
    using contract::contract;
    subscription(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        providers(receiver, receiver.value),
        config(name("settings"), name("settings").value),
        users(name("accounts"), name("accounts").value),
        apps(name("accounts"), name("accounts").value)
        {}

    ACTION reset();

    ACTION onperiod();

    ACTION create(name app, asset price);

    ACTION update(name app, bool active, asset price);

    ACTION increase(name from, name to, asset quantity, string memo);

    ACTION enable(name user, name app);

    ACTION disable(name user, name app);

    ACTION claimpayout(name app);
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);

    void check_user(name account);
    void check_asset(asset quantity);
    void deposit(asset quantity);
    void withdraw(name beneficiary, asset quantity);

    TABLE app_table {
      name account;

      uint64_t primary_key()const { return account.value; }
    };

    TABLE provider_table {
      name app;
      asset price;
      asset balance;
      bool active;
      uint64_t primary_key()const { return app.value; }
    };

    TABLE subscription_table {
      name user;
      asset deposit;
      asset invoice;
      bool active;
      uint64_t primary_key()const { return user.value; }
    };

    typedef eosio::multi_index<"providers"_n, provider_table> provider_tables;
    typedef eosio::multi_index<"subs"_n, subscription_table> subscription_tables;
    typedef eosio::multi_index<"apps"_n, app_table> app_tables;

    config_tables config;
    user_tables users;
    app_tables apps;
    provider_tables providers;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == name("transfer").value && code == "token"_n.value) {
        execute_action<subscription>(name(receiver), name(code), &subscription::increase);
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(subscription, (reset)(create)(update)(increase)(enable)(disable)(onperiod)(claimpayout))
        }
    }
}
