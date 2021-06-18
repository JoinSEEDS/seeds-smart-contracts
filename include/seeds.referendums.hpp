#include <contracts.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/user_table.hpp>

using namespace eosio;
using std::string;
using std::make_tuple;
using std::distance;

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

      ACTION update(
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

      ACTION updatevoice(uint64_t start, uint64_t batchsize);

      ACTION cancelvote(name voter, uint64_t referendum_id);

      ACTION onperiod();

      ACTION fixdesc(uint64_t id, string description); // temp for fixing description
      ACTION applyfixref(); // temp for fixing description
      ACTION backfixref(); // revert fixing description

  private:
    symbol seeds_symbol = symbol("SEEDS", 4);

    static constexpr name high_impact = "high"_n;
    static constexpr name medium_impact = "med"_n;
    static constexpr name low_impact = "low"_n;

    void give_voice();
    void run_testing();
    void run_active();
    void run_staged();
    void send_onperiod();
    void send_refund_stake(name account, asset quantity);
    void send_burn_stake(asset quantity);
    void send_change_setting(name setting_name, uint64_t setting_value);
    void check_citizen(name account);

    uint64_t get_quorum(const name & setting);
    uint64_t get_unity(const name & setting);

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
      uint64_t by_name()const { return setting_name.value; }
    };

    DEFINE_CONFIG_TABLE
        
    DEFINE_CONFIG_TABLE_MULTI_INDEX

    TABLE fix_refs_table {
        uint64_t ref_id;
        string description;
        uint64_t primary_key()const { return ref_id; }
    };
    typedef eosio::multi_index<"fixrefs"_n, fix_refs_table> fix_refs_tables;

    TABLE back_refs_table {
        uint64_t ref_id;
        string description;
        uint64_t primary_key()const { return ref_id; }
    };
    typedef eosio::multi_index<"backrefs"_n, back_refs_table> back_refs_tables;


    typedef multi_index<"balances"_n, balance_table> balance_tables;
    typedef multi_index<"referendums"_n, referendum_table,
      indexed_by<"byname"_n,
      const_mem_fun<referendum_table, uint64_t, &referendum_table::by_name>>
    > referendum_tables;
    typedef multi_index<"voters"_n, voter_table> voter_tables;

    balance_tables balances;
    config_tables config;
};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<referendums>(name(receiver), name(code), &referendums::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(referendums, (reset)(addvoice)(create)(update)(favour)(against)(cancelvote)(onperiod)(updatevoice)
        (fixdesc)(applyfixref)(backfixref)
        )
      }
  }
}
