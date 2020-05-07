#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <harvest_table.hpp>

using namespace eosio;
using namespace utils;
using std::string;

CONTRACT proposals : public contract {
  public:
      using contract::contract;
      proposals(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          props(receiver, receiver.value),
          voice(receiver, receiver.value),
          lastprops(receiver, receiver.value),
          cycle(receiver, receiver.value),
          config(contracts::settings, contracts::settings.value),
          users(contracts::accounts, contracts::accounts.value)
          {}

      ACTION reset();

      ACTION create(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund);

      ACTION cancel(uint64_t id);

      ACTION update(uint64_t id, string title, string summary, string description, string image, string url);

      ACTION stake(name from, name to, asset quantity, string memo);

      ACTION addvoice(name user, uint64_t amount);

      ACTION changetrust(name user, bool trust);

      ACTION favour(name user, uint64_t id, uint64_t amount);

      ACTION against(name user, uint64_t id, uint64_t amount);

      ACTION onperiod();

      ACTION decayvoice();

  private:
      symbol seeds_symbol = symbol("SEEDS", 4);
      
      void update_cycle();
      void update_voicedecay();
      uint64_t get_cycle_period_sec();
      uint64_t get_voice_decay_period_sec();

      void check_user(name account);
      void check_citizen(name account);
      void deposit(asset quantity);
      void withdraw(name account, asset quantity, name sender, string memo);
      void refund_staked(name beneficiary, asset quantity);
      void send_to_escrow(name fromfund, name recipient, asset quantity, string memo);
      void burn(asset quantity);
      void update_voice_table();

      TABLE config_table {
          name param;
          uint64_t value;
          uint64_t primary_key()const { return param.value; }
      };

      TABLE proposal_table {
          uint64_t id;
          name creator;
          name recipient;
          asset quantity;
          asset staked;
          bool executed;
          uint64_t total;
          uint64_t favour;
          uint64_t against;
          string title;
          string summary;
          string description;
          string image;
          string url;
          name status;
          name stage;
          name fund;
          uint64_t creation_date;
          uint64_t primary_key()const { return id; }
      };

      TABLE user_table {
        name account;
        name status;
        name type;
        string nickname;
        string image;
        string story;
        string roles;
        string skills;
        string interests;
        uint64_t reputation;
        uint64_t timestamp;
        uint64_t primary_key()const { return account.value; }
        uint64_t by_reputation()const { return reputation; }
      };

      TABLE vote_table {
          uint64_t proposal_id;
          name account;
          uint64_t amount;
          bool favour;
          uint64_t primary_key()const { return account.value; }
      };

      TABLE voice_table {
        name account;
        uint64_t balance;
        uint64_t primary_key()const { return account.value; }
      };

      TABLE last_proposal_table {
        name account;
        uint64_t proposal_id;
        uint64_t primary_key()const { return account.value; }
      };

    TABLE cycle_table {
      uint64_t propcycle; 
      uint64_t t_onperiod; // last time onperiod ran
      uint64_t t_voicedecay; // last time voice was decayed
    };
    
    typedef eosio::multi_index<"props"_n, proposal_table> proposal_tables;
    typedef eosio::multi_index<"votes"_n, vote_table> votes_tables;
    typedef eosio::multi_index<"config"_n, config_table> config_tables;
    typedef eosio::multi_index<"users"_n, user_table> user_tables;
    typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;
    typedef eosio::multi_index<"lastprops"_n, last_proposal_table> last_proposal_tables;
    typedef singleton<"cycle"_n, cycle_table> cycle_tables;
    typedef eosio::multi_index<"cycle"_n, cycle_table> dump_for_cycle;

    config_tables config;
    proposal_tables props;
    user_tables users;
    voice_tables voice;
    last_proposal_tables lastprops;
    cycle_tables cycle;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<proposals>(name(receiver), name(code), &proposals::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(proposals, (reset)(create)(update)(addvoice)(changetrust)(favour)(against)(onperiod)(decayvoice)(cancel))
      }
  }
}
