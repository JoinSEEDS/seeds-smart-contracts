#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  configure(name("propminstake"), 1 * 10000);
  configure(name("refsnewprice"), 25 * 10000);
  configure(name("refsmajority"), 80);
  configure(name("refsquorum"), 80);
  configure(name("propmajority"), 80);
  configure(name("propquorum"), 5);
  configure(name("propvoice"), 77); // voice base per period
  configure(name("hrvstreward"), 100000);
  configure(name("org.minplant"), 200 * 10000);
  configure(name("mooncyclesec"), utils::moon_cycle);
  configure(name("propdecaysec"), utils::seconds_per_day);
  
  // =====================================
  // referral rewards 
  // =====================================

  // community buiding points for referrer when user becomes resident
  configure(name("refcbp1.ind"), 1);
  // community buiding points for referrer when user becomes citizen
  configure(name("refcbp2.ind"), 1);

  // reputation points for referrer when user becomes resident
  configure(name("refrep1.ind"), 1);
  // reputation points for referrer when user becomes citizen
  configure(name("refrep2.ind"), 1);

  // reward for individual referrer when user becomes resident  
  configure(name("refrwd1.ind"), 10 * 10000); // 10 SEEDS
  // reward for individual referrer when user becomes citizen  
  configure(name("refrwd2.ind"), 15 * 10000); // 15 SEEDS

  // reward for org when user becomes resident
  configure(name("refrwd1.org"), 15 * 10000); // 8 SEEDS
  // reward for org when user becomes citizen
  configure(name("refrwd2.org"), 25 * 10000); // 12 SEEDS

  // reward for ambassador of referring org when user becomes resident
  configure(name("refrwd1.amb"), 4 * 10000); // 2 SEEDS
  // reward for abmassador of referring org when user becomes citizen
  configure(name("refrwd2.amb"), 6 * 10000); // 3 SEEDS

  // Maximum number of points a user can gain from others vouching for them
  configure(name("maxvouch"), 50);


  // contracts
  setcontract(name("accounts"), "accts.seeds"_n);
  setcontract(name("harvest"), "harvst.seeds"_n);
  setcontract(name("settings"), "settgs.seeds"_n);
  setcontract(name("proposals"), "funds.seeds"_n);
  setcontract(name("invites"), "invite.seeds"_n); // old invite contract
  setcontract(name("onboarding"), "join.seeds"_n); // new invite contract
  setcontract(name("referendums"), "rules.seeds"_n);
  setcontract(name("history"), "histry.seeds"_n);
  setcontract(name("policy"), "policy.seeds"_n);
  setcontract(name("token"), "token.seeds"_n);
  setcontract(name("acctcreator"), "free.seeds"_n);
}

void settings::configure(name param, uint64_t value) {
  require_auth(get_self());

  auto citr = config.find(param.value);

  if (citr == config.end()) {
    config.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  } else {
    config.modify(citr, _self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  }
}

void settings::setcontract(name contract, name account) {
  require_auth(get_self());

  auto citr = contracts.find(contract.value);

  if (citr == contracts.end()) {
    contracts.emplace(_self, [&](auto& item) {
      item.contract = contract;
      item.account = account;
    });
  } else {
    contracts.modify(citr, _self, [&](auto& item) {
      item.contract = contract;
      item.account = account;
    });
  }
}
