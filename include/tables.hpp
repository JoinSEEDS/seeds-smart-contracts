#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>


using eosio::name;
using eosio::asset;
using std::string;

namespace tables {
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

  TABLE balance_table {
      name account;
      asset planted;
      asset reward;

      uint64_t primary_key()const { return account.value; }
      uint64_t by_planted()const { return planted.amount; }
  };

}