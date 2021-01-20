#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

namespace graph {

  // document type
  static constexpr name QUEST = name("quest");
  static constexpr name MILESTONE = name("milestone");

  // graph edges
  static constexpr name OWNS = name("owns");
  static constexpr name OWNEDBY = name("ownedby");

  #define NOT_FOUND -1

  #define TYPE "type"
  #define TITLE "title"
  #define DESCRIPTION "description"
  #define AMOUNT "amount"
  #define FUND "fund"
  #define STATUS "status"
  #define STAGE "stage"
  #define PAYOUT_PERCENTAGE "payout_percentage"
  #define URL_DOCUMENTATION "url_documentation"

}
