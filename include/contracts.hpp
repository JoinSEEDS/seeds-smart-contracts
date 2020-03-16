#pragma once

#include <eosio/eosio.hpp>

using eosio::name;

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
  name forum = "forum.seeds"_n;
  name scheduler = "cycle.seeds"_n;
  name organization = "orgs.seeds"_n;
}
