#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  confwithdesc(name("propminstake"), 500 * 10000, "Minimum proposals stake threshold (in Seeds)", high_impact); // 500 Seeds
  confwithdesc(name("proppass.rep"), 10, "Reputation points for passed proposal", high_impact); // rep points for passed proposal
  
  confwithdesc(name("unity.high"), 80, "High unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.medium"), 70, "Medium unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.low"), 60, "Low unity threshold (in percentage)", high_impact);

  confwithdesc(name("quorum.high"), 7, "High threshold for quorum (in percentage)", high_impact);
  confwithdesc(name("quorum.mdium"), 5, "Medium threshold for quorum (in percentage)", high_impact);
  confwithdesc(name("quorum.low"), 3, "Low threshold for quorum (in percentage)", high_impact);

  confwithdesc(name("refsnewprice"), 25 * 10000, "Minimum price to create a referendum", high_impact);
  confwithdesc(name("refsmajority"), 80, "Majority referendums threshold", high_impact);
  confwithdesc(name("refsquorum"), 80, "Quorum referendums threshold", high_impact);
  confwithdesc(name("propmajority"), 80, "Majority proposals threshold", high_impact);
  confwithdesc(name("propquorum"), 5, "Quorum proposals threshold", high_impact);
  confwithdesc(name("propvoice"), 77, "Voice base per period", high_impact); // voice base per period
  confwithdesc(name("hrvstreward"), 100000, "Harvest reward", high_impact);
  confwithdesc(name("org.minplant"), 200 * 10000, "Minimum amount to create an organization (in Seeds)", high_impact);
  confwithdesc(name("mooncyclesec"), utils::moon_cycle, "Number of seconds a moon cycle has", high_impact);
  confwithdesc(name("propdecaysec"), utils::seconds_per_day, "Number of seconds per day", high_impact);
  confwithdesc(name("batchsize"), 200, "Number of elements per batch", high_impact);
  confwithdesc(name("bio.fee"), uint64_t(1000) * uint64_t(10000), "Minimum amount to create a bio region (in Seeds)", high_impact);

  confwithdesc(name("txlimit.mul"), 10, "Multiplier to calculate maximum number of transactions per user", high_impact);

  confwithdesc(name("txlimit.min"), 7, "Minimum number of transactions per user", high_impact);

  // Scheduler cycle
  confwithdesc(name("secndstoexec"), 60, "Seconds to execute", high_impact);

  // =====================================
  // citizenship path 
  // =====================================
  // Resident
  confwithdesc(name("res.plant"), 50 * 10000, "Minimum planted amount to become a Resident (in Seeds)", high_impact);   // min planted 50 Seeds
  confwithdesc(name("res.tx"), 10, "Minimum number of transactions to become a Resident", high_impact);              // min 10 transactions
  confwithdesc(name("res.referred"), 1, "Minimum number of referrals made to become a Resident", high_impact);         // min referred 1 other user
  confwithdesc(name("res.rep.pt"), 50, "Minimum reputation points to become a Resident", high_impact);          // min rep points

  // Citizen
  confwithdesc(name("cit.plant"), 200 * 10000, "Minimum planted amount to become a Citizen (in Seeds)", high_impact);  // min planted 200 Seeds
  confwithdesc(name("cit.tx"), 10, "Minimum transactions to become a Citizen", high_impact);              // min 10 transactions
  confwithdesc(name("cit.referred"), 3, "Minimum number of referrals made to become a Citizen", high_impact);         // min referred 3 other users
  confwithdesc(name("cit.ref.res"), 1, "Minimum residents o citizens referred to become a Citizen", high_impact);          // min referred 1 resident or citizen
  confwithdesc(name("cit.rep.sc"), 50, "Minimum reputation score to become a Citizen (not points)", high_impact);          // min reputation score (not points)
  confwithdesc(name("cit.age"), 59 * 24 * 60 * 60, "Minimum account age to become a Citizen (in seconds)", high_impact);  // min account age 2 cycles 


  // =====================================
  // referral rewards 
  // =====================================

  // community buiding points for referrer when user becomes resident
  confwithdesc(name("refcbp1.ind"), 1, "Community buiding points for referrer when user becomes resident", high_impact);

  // community buiding points for referrer when user becomes citizen
  confwithdesc(name("refcbp2.ind"), 1, "Community buiding points for referrer when user becomes citizen", high_impact);

  // reputation points for referrer when user becomes resident
  confwithdesc(name("refrep1.ind"), 1, "Reputation points for referrer when user becomes resident", high_impact);

  // reputation points for referrer when user becomes citizen
  confwithdesc(name("refrep2.ind"), 1, "Reputation points for referrer when user becomes citizen", high_impact);

  // reputation points for voting all active proposals
  confwithdesc(name("voterep1.ind"), 5, "Reputation points for voting all active proposals", high_impact);

  // reputation points for entering in the participants table
  confwithdesc(name("voterep2.ind"), 1, "Reputation points for entering in the participants table", high_impact);

  // reward for individual referrer when user becomes resident  
  confwithdesc(name("refrwd1.ind"), 1000 * 10000, "Reward for individual referrer when user becomes resident", high_impact); // 1000 SEEDS

  // reward for individual referrer when user becomes citizen  
  confwithdesc(name("refrwd2.ind"), 1500 * 10000, "Reward for individual referrer when user becomes citizen", high_impact); // 1500 SEEDS

  // reward for org when user becomes resident
  confwithdesc(name("refrwd1.org"), 800 * 10000, "Reward for org when user becomes resident", high_impact);  // 800 SEEDS

  // reward for org when user becomes citizen
  confwithdesc(name("refrwd2.org"), 1200 * 10000, "Reward for org when user becomes citizen", high_impact); // 1200 SEEDS

  // reward for ambassador of referring org when user becomes resident
  confwithdesc(name("refrwd1.amb"), 200 * 10000, "Reward for ambassador of referring org when user becomes resident", high_impact);  // 200 SEEDS

  // reward for abmassador of referring org when user becomes citizen
  confwithdesc(name("refrwd2.amb"), 300 * 10000, "Reward for abmassador of referring org when user becomes citizen", high_impact);  // 300 SEEDS

  // Maximum number of points a user can gain from others vouching for them
  confwithdesc(name("maxvouch"), 50, "Maximum number of points a user can gain from others vouching for them", high_impact);

  // vouch base reward resident
  confwithdesc(name("res.vouch"), 10, "Vouch base reward resident", high_impact);
  
  // vouch base reward citizen
  confwithdesc(name("cit.vouch"), 20, "Vouch base reward citizen", high_impact);

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

void settings::confwithdesc(name param, uint64_t value, string description, name impact) {
  require_auth(get_self());

  auto citr = config.find(param.value);

  if (citr == config.end()) {
    config.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
      item.description = description;
      item.impact = impact;
    });
  } else {
    config.modify(citr, _self, [&](auto& item) {
      item.param = param;
      item.value = value;
      item.description = description;
      item.impact = impact;
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
