#include <contracts.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;
using std::string;
using std::make_tuple;

#define MOVE_REFERENDUM(from, to) { \
  to.referendum_id = from->referendum_id; \
  to.created_at = from->created_at; \
  to.setting_value = from->setting_value; \
  to.favour = from->favour; \
  to.against = from->against; \
  to.setting_name = from->setting_name; \
  to.creator = from->creator; \
  to.staked = from->staked; \
  to.title = from->title; \
  to.summary = from->summary; \
  to.description = from->description; \
  to.image = from->image; \
  to.url = from->url; \
}

CONTRACT referendums : public contract {
  public:
      using contract::contract;
      referendums(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          balances(receiver, receiver.value),
          config(contracts::settings, contracts::settings.value)
          {}

      ACTION reset();

      ACTION create(
        name creator,
        name setting_name,
        uint64_t setting_value,
        string title,
        string summary,
        string description,
        string image,
        string url
      );

      ACTION favour(name voter, uint64_t referendum_id, uint64_t amount);

      ACTION against(name voter, uint64_t referendum_id, uint64_t amount);

      ACTION stake(name from, name to, asset quantity, string memo);

      ACTION addvoice(name account, uint64_t amount);

      ACTION cancelvote(name voter, uint64_t referendum_id);

      ACTION runcycle();
  private:
    symbol seeds_symbol = symbol("SEEDS", 4);

    bool is_approved(uint64_t favour, uint64_t against, uint64_t majority);
    void give_voice();
    void run_testing();
    void run_active();
    void run_staged();
    void send_runcycle();
    void send_refund_stake(name account, asset quantity);
    void send_burn_stake(asset quantity);
    void send_change_setting(name setting_name, uint64_t setting_value);

    TABLE voter_table {
      name account;
      uint64_t referendum_id;
      uint64_t amount;
      bool favoured;
      bool canceled;

      uint64_t primary_key()const { return account.value; }
    };

    TABLE balance_table {
      name account;
      asset stake;
      uint64_t voice;

      uint64_t primary_key()const { return account.value; }
    };

    TABLE referendum_table {
      uint64_t referendum_id;
      uint64_t created_at;
      uint64_t setting_value;
      uint64_t favour;
      uint64_t against;
      name setting_name;
      name creator;
      asset staked;
      string title;
      string summary;
      string description;
      string image;
      string url;

      uint64_t primary_key()const { return referendum_id; }
    };

    TABLE config_table {
        name param;
        uint64_t value;
        uint64_t primary_key()const { return param.value; }
    };


    typedef multi_index<"balances"_n, balance_table> balance_tables;
    typedef multi_index<"referendums"_n, referendum_table> referendum_tables;
    typedef multi_index<"voters"_n, voter_table> voter_tables;
    typedef multi_index<"config"_n, config_table> config_tables;

    balance_tables balances;
    config_tables config;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<referendums>(name(receiver), name(code), &referendums::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(referendums, (reset)(addvoice)(create)(favour)(against)(cancelvote)(runcycle))
      }
  }
}
