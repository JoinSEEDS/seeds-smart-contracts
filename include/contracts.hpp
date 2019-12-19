#pragma once

#include <eosio/eosio.hpp>

using eosio::name;

// Always comment out before checking in - we don't want to get this accidentally on mainnet 
//#define TESTNET = true; // this should be coming from env as compiler flag - only when deploying to testnet

#ifdef TESTNET
namespace contracts {
  name accounts =    "acctsxxseeds"_n;
  name harvest =     "harvstxseeds"_n;
  name settings =    "settgsxseeds"_n;
  name proposals =   "fundsxxseeds"_n;
  name invites =     "invitexseeds"_n;
  name referendums = "rulesxxseeds"_n;
  name history =     "histryxseeds"_n;
  name token =       "tokenxxseeds"_n;
  name tlostoken = "eosio.token"_n;
  name policy =      "policyxseeds"_n;
  name bank =        "systemxseeds"_n;
  name onboarding =  "joinxxxseeds"_n;
  name acctcreator = "freexxxseeds"_n;
}
#else
namespace contracts {
  name accounts = "accts.seeds"_n;
  name harvest = "harvst.seeds"_n;
  name settings = "settgs.seeds"_n;
  name proposals = "funds.seeds"_n;
  name invites = "invite.seeds"_n;
  name referendums = "rules.seeds"_n;
  name history = "histry.seeds"_n;
  name token = "token.seeds"_n;
  name tlostoken = "eosio.token"_n;
  name policy = "policy.seeds"_n;
  name bank = "system.seeds"_n;
  name onboarding = "join.seeds"_n;
  name acctcreator = "free.seeds"_n;
}
#endif
