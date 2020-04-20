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
  name exchange = "tlosto.seeds"_n;
  name escrow = "escrow.seeds"_n;
}
namespace bankaccts {
  name milestone = "milest.seeds"_n;
  name alliances = "allies.seeds"_n;
  name campaigns = "gift.seeds"_n;

  name hyphabank = "seeds.hypha"_n;
}
