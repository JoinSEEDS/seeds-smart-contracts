#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  configure(name("propminstake"), 500 * 10000); // 500 Seeds
  adddescriptn(name("propminstake"), "Minimum proposals stake threshold (in Seeds)");

  configure(name("proppass.rep"), 10); // rep points for passed proposal
  adddescriptn(name("proppass.rep"), "Reputation points for passed proposal");
  
  configure(name("unity.high"), 80);
  adddescriptn(name("unity.high"), "High unity threshold (in percentage)");

  configure(name("unity.medium"), 70);
  adddescriptn(name("unity.medium"), "Medium unity threshold (in percentage)");

  configure(name("unity.low"), 60);
  adddescriptn(name("unity.low"), "Low unity threshold (in percentage)");

  configure(name("refsnewprice"), 25 * 10000);
  adddescriptn(name("refsnewprice"), "Minimum price to create a referendum");

  configure(name("refsmajority"), 80);
  adddescriptn(name("refsmajority"), "Majority referendums threshold");

  configure(name("refsquorum"), 80);
  adddescriptn(name("refsquorum"), "Quorum referendums threshold");

  configure(name("propmajority"), 80);
  adddescriptn(name("propmajority"), "Majority proposals threshold");

  configure(name("propquorum"), 5);
  adddescriptn(name("propquorum"), "Quorum proposals threshold");

  configure(name("propvoice"), 77); // voice base per period
  adddescriptn(name("propvoice"), "Voice base per period");

  configure(name("hrvstreward"), 100000);
  adddescriptn(name("hrvstreward"), "Harvest reward");

  configure(name("org.minplant"), 200 * 10000);
  adddescriptn(name("org.minplant"), "Minimum amount to create an organization (in Seeds)");

  configure(name("mooncyclesec"), utils::moon_cycle);
  adddescriptn(name("mooncyclesec"), "Number of seconds a moon cycle has");

  configure(name("propdecaysec"), utils::seconds_per_day);
  adddescriptn(name("propdecaysec"), "Number of seconds per day");

  configure(name("batchsize"), 200);
  adddescriptn(name("batchsize"), "Number of elements per batch");

  configure(name("bio.fee"), uint64_t(1000) * uint64_t(10000));
  adddescriptn(name("bio.fee"), "Minimum amount to create a bio region (in Seeds)");

  configure(name("txlimit.mul"), 10);
  adddescriptn(name("txlimit.mul"), "Multiplier to calculate maximum number of transactions per user");

  configure(name("txlimit.min"), 7);
  adddescriptn(name("txlimit.min"), "Minimum number of transactions per user");

  // Scheduler cycle
  configure(name("secndstoexec"), 60);
  adddescriptn(name("secndstoexec"), "Seconds to execute");

  // =====================================
  // citizenship path 
  // =====================================
  // Resident
  configure(name("res.plant"), 50 * 10000);   // min planted 50 Seeds
  adddescriptn(name("res.plant"), "Minimum planted amount to become a Resident (in Seeds)");

  configure(name("res.tx"), 10);              // min 10 transactions
  adddescriptn(name("res.tx"), "Minimum number of transactions to become a Resident");

  configure(name("res.referred"), 1);         // min referred 1 other user
  adddescriptn(name("res.referred"), "Minimum number of referrals made to become a Resident");

  configure(name("res.rep.pt"), 50);          // min rep points
  adddescriptn(name("res.rep.pt"), "Minimum reputation points to become a Resident");

  // Citizen
  configure(name("cit.plant"), 200 * 10000);  // min planted 200 Seeds
  adddescriptn(name("cit.plant"), "Minimum planted amount to become a Citizen (in Seeds)");
  
  configure(name("cit.tx"), 10);              // min 10 transactions
  adddescriptn(name("cit.tx"), "Minimum transactions to become a Citizen");

  configure(name("cit.referred"), 3);         // min referred 3 other users
  adddescriptn(name("cit.referred"), "Minimum number of referrals made to become a Citizen");

  configure(name("cit.ref.res"), 1);          // min referred 1 resident or citizen
  adddescriptn(name("cit.ref.res"), "Minimum residents o citizens referred to become a Citizen");

  configure(name("cit.rep.sc"), 50);          // min reputation score (not points)
  adddescriptn(name("cit.rep.sc"), "Minimum reputation score to become a Citizen (not points)");

  configure(name("cit.age"), 59 * 24 * 60 * 60);  // min account age 2 cycles 
  adddescriptn(name("cit.age"), "Minimum account age to become a Citizen (in seconds)");


  // =====================================
  // referral rewards 
  // =====================================

  // community buiding points for referrer when user becomes resident
  configure(name("refcbp1.ind"), 1);
  adddescriptn(name("refcbp1.ind"), "Community buiding points for referrer when user becomes resident");

  // community buiding points for referrer when user becomes citizen
  configure(name("refcbp2.ind"), 1);
  adddescriptn(name("refcbp2.ind"), "Community buiding points for referrer when user becomes citizen");

  // reputation points for referrer when user becomes resident
  configure(name("refrep1.ind"), 1);
  adddescriptn(name("refrep1.ind"), "Reputation points for referrer when user becomes resident");

  // reputation points for referrer when user becomes citizen
  configure(name("refrep2.ind"), 1);
  adddescriptn(name("refrep2.ind"), "Reputation points for referrer when user becomes citizen");

  // reputation points for voting all active proposals
  configure(name("voterep1.ind"), 5);
  adddescriptn(name("voterep1.ind"), "Reputation points for voting all active proposals");

  // reputation points for entering in the participants table
  configure(name("voterep2.ind"), 1);
  adddescriptn(name("voterep2.ind"), "Reputation points for entering in the participants table");

  // reward for individual referrer when user becomes resident  
  configure(name("refrwd1.ind"), 1000 * 10000); // 1000 SEEDS
  adddescriptn(name("refrwd1.ind"), "Reward for individual referrer when user becomes resident");

  // reward for individual referrer when user becomes citizen  
  configure(name("refrwd2.ind"), 1500 * 10000); // 1500 SEEDS
  adddescriptn(name("refrwd2.ind"), "Reward for individual referrer when user becomes citizen");

  // reward for org when user becomes resident
  configure(name("refrwd1.org"), 800 * 10000);  // 800 SEEDS
  adddescriptn(name("refrwd1.org"), "Reward for org when user becomes resident");

  // reward for org when user becomes citizen
  configure(name("refrwd2.org"), 1200 * 10000); // 1200 SEEDS
  adddescriptn(name("refrwd2.org"), "Reward for org when user becomes citizen");

  // reward for ambassador of referring org when user becomes resident
  configure(name("refrwd1.amb"), 200 * 10000);  // 200 SEEDS
  adddescriptn(name("refrwd1.amb"), "Reward for ambassador of referring org when user becomes resident");

  // reward for abmassador of referring org when user becomes citizen
  configure(name("refrwd2.amb"), 300 * 10000);  // 300 SEEDS
  adddescriptn(name("refrwd2.amb"), "Reward for abmassador of referring org when user becomes citizen");

  // Maximum number of points a user can gain from others vouching for them
  configure(name("maxvouch"), 50);
  adddescriptn(name("maxvouch"), "Maximum number of points a user can gain from others vouching for them");

  // vouch base reward resident
  configure(name("res.vouch"), 10);
  adddescriptn(name("res.vouch"), "Vouch base reward resident");
  
  // vouch base reward citizen
  configure(name("cit.vouch"), 20);
  adddescriptn(name("cit.vouch"), "Vouch base reward citizen");

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

void settings::adddescriptn(name param, string description) {
  require_auth(get_self());

  auto citr = config.find(param.value);
  check(citr != config.end(), "The " + param.to_string() + " parameter is not in the config table.");

  config.modify(citr, _self, [&](auto & item){
    item.description = description;
  });
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
