#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosio.token.hpp>

using namespace eosio;
using std::string;
using std::count;

CONTRACT proposals : public contract {
  public:
      using contract::contract;
      proposals(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          props(receiver, receiver.value),
          config(name("seedsettings"), name("settings").value),
          users(name("seedsaccnts3"), name("accounts").value)
          {}

      ACTION reset();

      ACTION create(name creator, name recipient, asset quantity, string memo);

      ACTION vote(name voter, uint64_t id);

      ACTION execute(uint64_t id);
  private:
      symbol seeds_symbol = symbol("SEEDS", 4);

      void check_user(name account);
      void check_asset(asset quantity);
      void withdraw(name account, asset quantity);

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
          string memo;
          bool executed;
          uint64_t votes;
          uint64_t primary_key()const { return id; }
      };

      TABLE user_table {
          name account;
          uint64_t primary_key()const { return account.value; }
      };

      TABLE vote_table {
          name account;
          uint64_t primary_key()const { return account.value; }
      };

      typedef eosio::multi_index<"props"_n, proposal_table> proposal_tables;
      typedef eosio::multi_index<"votes"_n, vote_table> votes_tables;
      typedef eosio::multi_index<"config"_n, config_table> config_tables;
      typedef eosio::multi_index<"users"_n, user_table> user_tables;

      config_tables config;
      proposal_tables props;
      user_tables users;
};

EOSIO_DISPATCH(proposals, (reset)(create)(vote)(execute));
