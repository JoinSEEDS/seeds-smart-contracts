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
  configure(name("propvoice"), 20); // voice base per period
  configure(name("hrvstreward"), 100000);
  configure(name("org.minplant"), 200 * 10000);

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
