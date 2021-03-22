#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  confwithdesc(name("propminstake"), uint64_t(555) * uint64_t(10000), "[Legacy] ]Minimum proposals stake threshold (in Seeds)", high_impact); 
  confwithdesc(name("propmaxstake"), uint64_t(75000) * uint64_t(10000), "[Legacy] Max proposals stake 11,111 threshold (in Seeds)", high_impact);
  confwithdesc(name("propstakeper"), 3, "[Legacy] Proposal funding fee in % - 3%", high_impact);

  confwithdesc(name("prop.cmp.cap"), uint64_t(75000) * uint64_t(10000), "Campains proposals cap stake", high_impact);
  confwithdesc(name("prop.al.cap"), uint64_t(15000) * uint64_t(10000), "Alliance proposals cap stake", high_impact);

  confwithdesc(name("prop.cmp.min"), uint64_t(555) * uint64_t(10000), "Campains proposals minimum stake", high_impact);
  confwithdesc(name("prop.al.min"), uint64_t(555) * uint64_t(10000), "Alliance proposals minimum stake", high_impact);

  confwithdesc(name("prop.cmp.pct"), uint64_t(2.5 * 10000), "Campains proposals funding fee in % - 2.5% [x 10,000 for 4 digits of precision]", high_impact);
  confwithdesc(name("prop.al.pct"), uint64_t(1 * 10000), "Alliance proposals funding fee in % - 1% [x 10,000 for 4 digits of precision]", high_impact);

  confwithdesc(name("proppass.rep"), 10, "Reputation points for passed proposal", high_impact); // rep points for passed proposal

  confwithdesc(name("prop.cyc.qb"), 2, "Prop cycles to take into account for calculating quorum basis", high_impact);
  
  confwithdesc(name("unity.high"), 90, "High unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.medium"), 85, "Medium unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.low"), 80, "Low unity threshold (in percentage)", high_impact);

  confwithdesc(name("quorum.high"), 20, "High threshold for quorum (in percentage of total citizens)", high_impact);
  confwithdesc(name("quorum.med"), 10, "Medium threshold for quorum (in percentage total citizens)", high_impact);
  confwithdesc(name("quorum.low"), 5, "Low threshold for quorum (in percentage total citizens)", high_impact);

  confwithdesc(name("refsnewprice"), 1111 * 10000, "Minimum price to create a referendum - 1111.0000 SEEDS", high_impact);
  confwithdesc(name("refsmajority"), 80, "Majority referendums threshold", high_impact);
  confwithdesc(name("refsquorum"), 80, "Quorum referendums threshold", high_impact);
  confwithdesc(name("propmajority"), 90, "Majority proposals threshold", high_impact);
  confwithdesc(name("propquorum"), 5, "Quorum proposals threshold", high_impact); // Deprecated

  confwithdesc(name("quorum.base"), 100, "Quorum base percentage = 90% / number of proposals.", high_impact);
  confwithdesc(name("quor.min.pct"), 7, "Quorum percentage lower cap - quorum required between 5% and 20%", high_impact);
  confwithdesc(name("quor.max.pct"), 40, "Quorum percentage upper cap- quorum required between 5% and 20%", high_impact);

  confwithdesc(name("propvoice"), 77, "Voice base per period", high_impact); // voice base per period
  confwithdesc(name("hrvstreward"), 100000, "Harvest reward", high_impact);
  confwithdesc(name("mooncyclesec"), utils::moon_cycle, "Number of seconds a moon cycle has", high_impact);
  confwithdesc(name("batchsize"), 200, "Number of elements per batch", high_impact);
  confwithdesc(name("bio.fee"), uint64_t(1000) * uint64_t(10000), "Minimum amount to create a bio region (in Seeds)", high_impact);
  confwithdesc(name("vdecayprntge"), 15, "The percentage of voice decay (in percentage)", high_impact);
  confwithdesc(name("decaytime"), utils::proposal_cycle / 2, "Minimum amount of seconds before start voice decay", high_impact);
  confwithdesc(name("propdecaysec"), utils::seconds_per_day, "Minimum amount of seconds before execute a new voice decay", high_impact);
  confwithdesc(name("inact.cyc"), 3, "Number of cycles until a voter is considered inactive and no longer counts towards quorum", high_impact);
  confwithdesc(name("propcyclesec"), utils::proposal_cycle, "Proposal cycle lengs in seconds", high_impact);

  confwithdesc(name("dlegate.dpth"), 100, "Maximum depth for delegating the voice", high_impact);

  confwithdesc(name("txlimit.mul"), 10, "Multiplier to calculate maximum number of transactions per user", high_impact);

  confwithdesc(name("i.trx.max"), uint64_t(1777) * uint64_t(10000), "Individuals: Maximum points for a transaction", high_impact);
  confwithdesc(name("i.trx.div"), 26, "Individuals: Transaction diversity not more than 26 from 1 user is counted", high_impact);
  
  confwithdesc(name("org.trx.max"), uint64_t(1777) * uint64_t(10000), "Organization: Maximum points for a transaction", high_impact);
  confwithdesc(name("org.trx.div"), 26, "Organization: Transaction diversity not more than 26 from 1 other is counted", high_impact);

  confwithdesc(name("txlimit.min"), 7, "Minimum number of transactions per user", high_impact);

  confwithdesc(name("htry.trx.max"), 2, "Maximum number of transactions to take into account for transaction score between to users per day", high_impact);
  confwithdesc(name("qev.trx.cap"), uint64_t(1777) * uint64_t(10000), "Maximum number of seeds to take into account as qualifying volume", high_impact);

  // Harvest distribution
  confwithdesc(name("hrvst.users"), 300000, "Percentage of the harvest that Residents/Citizens will receive (4 decimals of precision)", high_impact);
  confwithdesc(name("hrvst.bios"), 300000, "Percentage of the harvest that Bioregions will receive (4 decimals of precision)", high_impact);
  confwithdesc(name("hrvst.orgs"), 200000, "Percentage of the harvest that Organizations will receive (4 decimals of precision)", high_impact);
  confwithdesc(name("hrvst.global"), 200000, "Percentage of the harvest that Global G-DHO will receive (4 decimals of precision)", high_impact);
  
  // Organizations
  confwithdesc(name("org.minplant"), 200 * 10000, "Minimum amount to create an organization (in Seeds)", high_impact);
  
  confwithdesc(name("org.minsub"), 7, "Minimum amount of rating points a user can take from an org", high_impact);
  confwithdesc(name("org.maxadd"), 7, "Maximum amount of rating points a user can give to an org", high_impact);

  // replace this single rating with the below
  // confwithdesc(name("org.rgen.min"), 1000, "Minimum regen points an organization must have to be ranked", high_impact);
  
  // user rating threshold when an org can become a (reputable/sustainable/regenerative/thrivable) org
  confwithdesc(name("org.rated.5"), 1000, "Thrivable organization rating threshold", low_impact);
  confwithdesc(name("org.rated.4"), 200, "Regenerative organization rating threshold", low_impact);
  confwithdesc(name("org.rated.3"), 100, "Sustainable organization rating threshold", low_impact);
  confwithdesc(name("org.rated.2"), 50, "Reputable organization rating threshold", low_impact);


  // Resident orgs
  confwithdesc(name("rep.minplnt"), 400 * 10000, "Minimum amount planted to become a Reputable Organization (in Seeds)", high_impact);
  confwithdesc(name("rep.minrank"), 50, "Minimum rank an organization must have to become a Reputable Organization", high_impact);
  confwithdesc(name("rep.refrred"), 10, "Minimum number of referrals to become a Reputable Organization", high_impact);
  confwithdesc(name("rep.resref"), 5, "Minimum number of residents or citizens referred to become a Reputable Organization", high_impact);
  confwithdesc(name("rep.mintrx"), 5, "Minimum number of exchanged transactions with Reputable/Regenerative organizations or Citizens to become a Reputable Organization", high_impact);

  // Regenerative orgs
  confwithdesc(name("rgen.minplnt"), 400 * 10000, "Minimum amount planted to become a Regenerative Organization (in Seeds)", high_impact);
  confwithdesc(name("rgen.minrank"), 50, "Minimum rank an organization must have to become a Regenerative Organization", high_impact);
  confwithdesc(name("rgen.refrred"), 10, "Minimum number of referrals to become a Regenerative Organization", high_impact);
  confwithdesc(name("rgen.resref"), 5, "Minimum number of residents or citizens referred to become a Regenerative Organization", high_impact);
  confwithdesc(name("rgen.mintrx"), 5, "Minimum number of exchanged transactions with Reputable/Regenerative organizations or Citizens to become a Regenerative Organization", high_impact);

  // Scheduler cycle
  confwithdesc(name("secndstoexec"), 60, "Seconds to execute", high_impact);

  // =====================================
  // citizenship path 
  // =====================================

  // Visitor
  confwithdesc(name("vis.plant"), 5 * 10000, "Minimum planted amount to become a Visitor (in Seeds)", high_impact);   // min planted 5 Seeds

  // Resident
  confwithdesc(name("res.plant"), 50 * 10000, "Minimum planted amount to become a Resident (in Seeds)", high_impact);   // min planted 50 Seeds
  confwithdesc(name("res.tx"), 1, "Minimum number of transactions to become a Resident", high_impact);              // min 10 transactions
  confwithdesc(name("res.referred"), 1, "Minimum number of referrals made to become a Resident", high_impact);         // min referred 1 other user
  confwithdesc(name("res.rep.pt"), 50, "Minimum reputation points to become a Resident", high_impact);          // min rep points

  // Citizen
  confwithdesc(name("cit.plant"), 200 * 10000, "Minimum planted amount to become a Citizen (in Seeds)", high_impact);  // min planted 200 Seeds
  confwithdesc(name("cit.tx"), 5, "Minimum transactions to become a Citizen", high_impact);              // min 10 transactions
  confwithdesc(name("cit.referred"), 3, "Minimum number of referrals made to become a Citizen", high_impact);         // min referred 3 other users
  confwithdesc(name("cit.ref.res"), 1, "Minimum residents or citizens referred to become a Citizen", high_impact);          // min referred 1 resident or citizen
  confwithdesc(name("cit.rep.sc"), 50, "Minimum reputation score to become a Citizen (not points)", high_impact);          // min reputation score (not points)
  confwithdesc(name("cit.age"), 59 * 24 * 60 * 60, "Minimum account age to become a Citizen (in seconds)", high_impact);  // min account age 2 cycles 


  // =====================================
  // referral rewards 
  // =====================================

  // reputation points for referrer when user becomes resident
  confwithdesc(name("refrep1.ind"), 1, "Reputation points for referrer when user becomes resident", high_impact);

  // reputation points for referrer when user becomes citizen
  confwithdesc(name("refrep2.ind"), 1, "Reputation points for referrer when user becomes citizen", high_impact);

  // reputation points for voting all active proposals
  confwithdesc(name("voterep1.ind"), 5, "Reputation points for voting all active proposals", high_impact);

  // reputation points for entering in the participants table
  confwithdesc(name("voterep2.ind"), 1, "Reputation points for entering in the participants table", high_impact);

  // percentage of reputation points earned when trust is delegated
  confwithdesc(name("votedel.mul"), 80, "Percentage of reputation points earned when trust is delegated", high_impact);

  // reward for individual referrer when user becomes resident  
  confwithdesc(name("refrwd1.ind"), 750 * 10000, "Reward for individual referrer when user becomes resident", high_impact);

  // reward for individual referrer when user becomes citizen  
  confwithdesc(name("refrwd2.ind"), 1250 * 10000, "Reward for individual referrer when user becomes citizen", high_impact);

  // reward for org when user becomes resident
  confwithdesc(name("refrwd1.org"), 640 * 10000, "Reward for org when user becomes resident", high_impact);

  // reward for org when user becomes citizen
  confwithdesc(name("refrwd2.org"), 960 * 10000, "Reward for org when user becomes citizen", high_impact);

  // reward for ambassador of referring org when user becomes resident
  confwithdesc(name("refrwd1.amb"), 150 * 10000, "Reward for ambassador of referring org when user becomes resident", high_impact);

  // reward for abmassador of referring org when user becomes citizen
  confwithdesc(name("refrwd2.amb"), 250 * 10000, "Reward for abmassador of referring org when user becomes citizen", high_impact);

  // Degrading SEEDS rewards for referrals

  // min reward for individual referrer when user becomes resident  
  confwithdesc(name("minrwd1.ind"), 20 * 10000, "Minimum reward for individual referrer when user becomes resident", high_impact);

  // min reward for individual referrer when user becomes citizen  
  confwithdesc(name("minrwd2.ind"), 30 * 10000, "Minimum reward for individual referrer when user becomes citizen", high_impact);

  // min reward for org when user becomes resident
  confwithdesc(name("minrwd1.org"), 16 * 10000, "Minimum reward for org when user becomes resident", high_impact);

  // min reward for org when user becomes citizen
  confwithdesc(name("minrwd2.org"), 24 * 10000, "Minimum reward for org when user becomes citizen", high_impact);

  // min reward for ambassador of referring org when user becomes resident
  confwithdesc(name("minrwd1.amb"), 4 * 10000, "Minimum reward for ambassador of referring org when user becomes resident", high_impact);

  // min reward for abmassador of referring org when user becomes citizen
  confwithdesc(name("minrwd2.amb"), 6 * 10000, "Minimum reward for abmassador of referring org when user becomes citizen", high_impact);

  // reward decay for individual referrer when user becomes resident  
  confwithdesc(name("decrwd1.ind"), 110000 * 10000, "Reward decay for individual referrer when user becomes resident", high_impact);

  // min reward decay for individual referrer when user becomes citizen  
  confwithdesc(name("decrwd2.ind"), 75000 * 10000, "Reward decay for individual referrer when user becomes citizen", high_impact);

  // reward decay for org when user becomes resident
  confwithdesc(name("decrwd1.org"), 88000 * 10000, "Reward decay for org when user becomes resident", high_impact);

  // reward decay for org when user becomes citizen
  confwithdesc(name("decrwd2.org"), 60000 * 10000, "Reward decay for org when user becomes citizen", high_impact);

  // reward decay for ambassador of referring org when user becomes resident
  confwithdesc(name("decrwd1.amb"), 22000 * 10000, "Reward decay for ambassador of referring org when user becomes resident", high_impact);

  // reward decay for abmassador of referring org when user becomes citizen
  confwithdesc(name("decrwd2.amb"), 15000 * 10000, "Reward decay for abmassador of referring org when user becomes citizen", high_impact);


  // =====================================
  // vouching rewards 
  // =====================================

  // Maximum number of points a user can gain from others vouching for them
  confwithdesc(name("maxvouch"), 50, "Maximum number of points a user can gain from others vouching for them", high_impact);

  // vouch base reward resident
  confwithdesc(name("res.vouch"), 10, "Vouch base reward resident", high_impact);
  
  // vouch base reward citizen
  confwithdesc(name("cit.vouch"), 20, "Vouch base reward citizen", high_impact);

  // community buiding points for referrer when user becomes resident
  confwithdesc(name("ref.cbp1.ind"), 2, "Community buiding points for referrer when user becomes resident", high_impact);

  // community buiding points for referrer when user becomes citizen
  confwithdesc(name("ref.cbp2.ind"), 2, "Community buiding points for referrer when user becomes citizen", high_impact);

  // community buiding points for voucher when user referred reputable org
  confwithdesc(name("ref.org1.cbp1"), 8, "Referred reputable organization", high_impact);

  // community buiding points for org referrer when referred user becomes resident
  confwithdesc(name("refcbp1.org"), 2, "Org community building points reward when user becomes resident", high_impact);

  // community buiding points for org referrer when user becomes citizen
  confwithdesc(name("refcbp2.org"), 2, "Org community building points reward when user becomes citizen", high_impact);

  // =====================================
  // flag points
  // =====================================
  confwithdesc(name("flag.thresh"), 100, "Max number of flag points before lossing reputation", high_impact);
  confwithdesc(name("flag.base.r"), 10, "Base flag points a resident can give", high_impact);
  confwithdesc(name("flag.base.c"), 20, "Base flag points a citizen can give", high_impact);
  conffloatdsc(name("flag.vouch.p"), 0.5, "Percentage of the reputation the vouchers will lose", high_impact);


  // =====================================
  // forum 
  // =====================================
  confwithdesc(name("forum.maxp"), 100000, "Max points the user will get for voting", high_impact);
  confwithdesc(name("forum.vpb"), 70000, "Vote Base Points multiplier", high_impact);
  confwithdesc(name("forum.cutoff"), 280000, "Minimum value to start the decay", high_impact);
  confwithdesc(name("forum.cutzro"), 5000, "Minimum value to set vote power to zero", high_impact);
  confwithdesc(name("forum.dp"), 9500, "Depreciation multiplier (four decimal precision)", high_impact);
  confwithdesc(name("forum.dps"), 5, "Depreciation frequency (in days)", high_impact);


  // =====================================
  // transaction multipliers
  // =====================================
  conffloatdsc(name("local.mul"), 1.5, "Transaction multiplier for exchanging within the same bioregion", high_impact);
  conffloatdsc(name("regen.mul"), 1.5, "Transaction multiplier for exchanging with a regenerative organization", high_impact);


  conffloatdsc(name("cyctrx.trail"), 3.0, "Number of cycles to take into account for calculating transaction points for individuals and orgs", high_impact);

  // =====================================
  // bioregion
  // =====================================
  conffloatdsc(name("bio.vote.del"), 1.0, "Number of moon cycles to wait before user can vote or join another bioregion", high_impact);

  // =====================================
  // gratitude 
  // =====================================
  confwithdesc(name("gratz.gen"), 100 * 10000, "Base quantity of gratitude tokens received per cycle", high_impact);

  // =====================================
  // onboarding/invite
  // =====================================
  confwithdesc(name("inv.max.rwrd"), 1000 * 10000, "Reward the owner of a campaigns receives per invite", high_impact);
  confwithdesc(name("inv.min.plnt"), 5 * 10000, "Minimum amount planted per invite", high_impact);

  // contracts
  setcontract(name("accounts"), "accts.seeds"_n);
  setcontract(name("harvest"), "harvst.seeds"_n);
  setcontract(name("settings"), "settgs.seeds"_n);
  setcontract(name("proposals"), "funds.seeds"_n);
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

  auto fitr = configfloat.find(param.value);
  check(fitr == configfloat.end(), param.to_string() + ", this parameter is defined as floating point");

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

void settings::conffloat(name param, double value) {
  require_auth(get_self());

  auto citr = configfloat.find(param.value);

  auto iitr = config.find(param.value);
  check(iitr == config.end(), "this parameter is defined as an integer");

  if (citr == configfloat.end()) {
    configfloat.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  } else {
    configfloat.modify(citr, _self, [&](auto& item) {
      item.param = param;
      item.value = value;
    });
  }
}

void settings::confwithdesc(name param, uint64_t value, string description, name impact) {
  require_auth(get_self());

  auto citr = config.find(param.value);

  auto fitr = configfloat.find(param.value);
  check(fitr == configfloat.end(), param.to_string() + ", this parameter is defined as floating point");

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

void settings::conffloatdsc(name param, double value, string description, name impact) {
  require_auth(get_self());

  auto citr = configfloat.find(param.value);

  auto iitr = config.find(param.value);
  check(iitr == config.end(), "this parameter is defined as an integer");

  if (citr == configfloat.end()) {
    configfloat.emplace(_self, [&](auto& item) {
      item.param = param;
      item.value = value;
      item.description = description;
      item.impact = impact;
    });
  } else {
    configfloat.modify(citr, _self, [&](auto& item) {
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
