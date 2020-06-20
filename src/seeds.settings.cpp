#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  configure(name("propminstake"), 500 * 10000); // 500 Seeds
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
  
  // Scheduler cycle
  configure(name("secndstoexec"), 60);

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
  configure(name("refrwd1.ind"), 1000 * 10000); // 1000 SEEDS
  // reward for individual referrer when user becomes citizen  
  configure(name("refrwd2.ind"), 1500 * 10000); // 1500 SEEDS

  // reward for org when user becomes resident
  configure(name("refrwd1.org"), 800 * 10000);  // 800 SEEDS
  // reward for org when user becomes citizen
  configure(name("refrwd2.org"), 1200 * 10000); // 1200 SEEDS

  // reward for ambassador of referring org when user becomes resident
  configure(name("refrwd1.amb"), 200 * 10000);  // 200 SEEDS
  // reward for abmassador of referring org when user becomes citizen
  configure(name("refrwd2.amb"), 300 * 10000);  // 300 SEEDS

  // Maximum number of points a user can gain from others vouching for them
  configure(name("maxvouch"), 50);

  // vouch base reward resident
  configure(name("res.vouch"), 10);
  
  // vouch base reward citizen
  configure(name("cit.vouch"), 20);

  // citizenship 

  // min age of account to be citizen
  configure(name("cit.minage"), 2 * 29.5 * 24 * 60 * 60); // 2 cycles in seconds


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
