#include <seeds.settings.hpp>

void settings::reset() {
  require_auth(_self);

  // config
  // Description has multiplier in brackets [x10000] for example
  confwithdesc(name("propminstake"), uint64_t(555) * uint64_t(10000), "Minimum proposals stake threshold (in Seeds)", high_impact); 
  confwithdesc(name("propmaxstake"), uint64_t(11111) * uint64_t(10000), "Max proposals stake 11,111 threshold (in Seeds)", high_impact);
  confwithdesc(name("propstakeper"), 5, "Proposal funding fee in % - 5%", high_impact); 
  confwithdesc(name("prop.maxask"), uint64_t(250000) * uint64_t(10000), "Max first ask 250,000 Seeds", high_impact);

  confwithdesc(name("proppass.rep"), 10, "Reputation points for passed proposal", high_impact); 
  confwithdesc(name("terminat.rep"), -50, "Reputation points penalty for terminated proposal", high_impact); 
  
  confwithdesc(name("unity.high"), 80, "High unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.medium"), 70, "Medium unity threshold (in percentage)", high_impact);
  confwithdesc(name("unity.low"), 60, "Low unity threshold (in percentage)", high_impact);

  confwithdesc(name("quorum.high"), 10, "Referendums: High threshold for quorum (in percentage)", high_impact);
  confwithdesc(name("quorum.med"), 7, "Referendums: Medium threshold for quorum (in percentage)", high_impact);
  confwithdesc(name("quorum.low"), 5, "Referendums: Low threshold for quorum (in percentage)", high_impact);
  
  confwithdesc(name("unity.high"), 90, "Referendums: High threshold for unity (in percentage)", high_impact);
  confwithdesc(name("unity.med"), 85, "Referendums: Medium threshold for unity (in percentage)", high_impact);
  confwithdesc(name("unity.low"), 80, "Referendums: Low threshold for unity (in percentage)", high_impact);

  confwithdesc(name("refsnewprice"), 1111 * 10000, "Minimum price to create a referendum", high_impact);
  confwithdesc(name("refsmajority"), 90, "Deprecated - Majority referendums threshold", high_impact);
  confwithdesc(name("refsquorum"), 80, "Deprecated - Quorum referendums threshold", high_impact);
  confwithdesc(name("propmajority"), 80, "Majority proposals threshold", high_impact);
  confwithdesc(name("propquorum"), 5, "Deprecated: Quorum proposals threshold", high_impact);
  confwithdesc(name("propvoice"), 77, "Deprecated", high_impact); // No longer used - remove

  confwithdesc(name("hrvstreward"), 100000, "Harvest reward", high_impact);
  confwithdesc(name("mooncyclesec"), utils::moon_cycle, "Number of seconds a moon cycle has", high_impact);
  confwithdesc(name("batchsize"), 200, "Number of elements per batch", high_impact);
  
  // Bioregions
  confwithdesc(name("bio.fee"), uint64_t(1000) * uint64_t(10000), "Minimum amount to create a bio region (in Seeds)", high_impact);
  confwithdesc(name("bio.cit"), 144, "Number of citizens who need to join a bioregion to activate it", high_impact);
  confwithdesc(name("bio.amb"), 3, "Number of ambassadors that need to bless a bioregion", high_impact);
  
  confwithdesc(name("vdecayprntge"), 15, "The percentage of voice decay (in percentage)", high_impact);
  confwithdesc(name("decaytime"), utils::proposal_cycle / 2, "Minimum amount of seconds before start voice decay", high_impact);
  confwithdesc(name("propdecaysec"), utils::seconds_per_day, "Minimum amount of seconds before execute a new voice decay", high_impact);

  // Transaction limits
  confwithdesc(name("txlimit.mul"), 10, "Multiplier to calculate maximum number of transactions per user", high_impact);

  confwithdesc(name("i.trx.max"), uint64_t(1777) * uint64_t(10000), "Individuals: Maximum points for a transaction", high_impact);
  confwithdesc(name("i.trx.div"), 26, "Individuals: Transaction diversity not more than 26 from 1 user is counted", high_impact);
  
  confwithdesc(name("org.trx.max"), uint64_t(1777) * uint64_t(10000), "Organization: Maximum points for a transaction", high_impact);
  confwithdesc(name("org.trx.div"), 26, "Organization: Transaction diversity not more than 26 from 1 other is counted", high_impact);

  confwithdesc(name("txlimit.min"), 7, "Minimum number of transactions per user", high_impact);

  // Organizations
  confwithdesc(name("org.minplant"), 200 * 10000, "Minimum amount to create an organization (in Seeds)", high_impact);
  confwithdesc(name("org.rgen.min"), 1000, "Minimum regen points an organization must have to be ranked", high_impact);
  confwithdesc(name("org.minsub"), 7, "Minimum amount of points a user can take from an org", high_impact);
  confwithdesc(name("org.maxadd"), 7, "Maximum amount of points a user can give to an org", high_impact);

  // Resident orgs
  confwithdesc(name("rep.minplnt"), 400 * 10000, "Minimum amount planted to become a Reputable Organization (in Seeds)", medium_impact);
  confwithdesc(name("rep.minrank"), 60, "Minimum rank an organization must have to become a Reputable Organization", medium_impact);
  confwithdesc(name("rep.refrred"), 10, "Minimum number of referrals to become a Reputable Organization", medium_impact);
  confwithdesc(name("rep.resref"), 5, "Minimum number of residents or citizens referred to become a Reputable Organization", medium_impact);
  confwithdesc(name("rep.mintrx"), 10, "Minimum number of exchanged transactions with Reputable/Regenerative organizations or Citizens to become a Reputable Organization", medium_impact);

  // Regenerative orgs
  confwithdesc(name("rgen.minplnt"), 400 * 10000, "Minimum amount planted to become a Regenerative Organization (in Seeds)", medium_impact);
  confwithdesc(name("rgen.minrank"), 50, "Minimum rank an organization must have to become a Regenerative Organization", medium_impact);
  confwithdesc(name("rgen.refrred"), 10, "Minimum number of referrals to become a Regenerative Organization", medium_impact);
  confwithdesc(name("rgen.resref"), 5, "Minimum number of residents or citizens referred to become a Regenerative Organization", medium_impact);
  confwithdesc(name("rgen.mintrx"), 5, "Minimum number of exchanged transactions with Reputable/Regenerative organizations or Citizens to become a Regenerative Organization", medium_impact);

  // Scheduler cycle
  confwithdesc(name("secndstoexec"), 60, "Seconds to execute", high_impact);

  // =====================================
  // Qualifying Transactions 
  // =====================================
  conffloat(name("tx.cycle"), 3.0 , "Transaction Volume Cycle length / lookback period", high_impact);   
  conffloat(name("local.mul"), 1.5 , "Local Economy Multiplier - Multiplier for exchanging within Bioregion", high_impact);   
  conffloat(name("regen.mul"), 1.5 , "Regenerative Organisation Multiplier - multiplier for exchanging with Regenerative organisations", high_impact);   

  // =====================================
  // reputation points 
  // =====================================
  conffloat(name("flag.thresh"), -50.0 , "Flag threshold - nothing happens until an account is flagged with threshold negative points.", high_impact); 
  
  conffloat(name("flag.bas.res"), -10.0 , "Base value of flag rep points subtracted", high_impact); 
  
  conffloat(name("flag.bas.cit"), -20.0 , "Base value of flag rep points subtracted", high_impact); 

  // =====================================
  // Vote governance delegation multiplier 
  // =====================================
  conffloat(name("votedel.mul"), 0.8 , "Rep points multiplier - delegator get 80% of original rewards, delegate gets the other 20%", high_impact); 


  // =====================================
  // Proposal cyle length
  // =====================================
  conffloat(name("prop.cyc.len"), 0.5 , "Length of proposal cycle expressed in moon cycles - e.g. 1/2 moon cycle", high_impact); 

  // =====================================
  // citizenship path 
  // =====================================

  confwithdesc(name("vis.plant"), 5 * 10000, "Minimum planted amount to participate in Seeds [x10000]", high_impact);   

  // Resident
  confwithdesc(name("res.plant"), 50 * 10000, "Minimum planted amount to become a Resident (in Seeds)", high_impact);   
  confwithdesc(name("res.tx"), 10, "Minimum number of transactions to become a Resident", high_impact);              // min 10 transactions
  confwithdesc(name("res.referred"), 1, "Minimum number of referrals made to become a Resident", high_impact);         // min referred 1 other user
  confwithdesc(name("res.rep.pt"), 50, "Minimum reputation points to become a Resident", high_impact);          // min rep points

  // Citizen
  confwithdesc(name("cit.plant"), 200 * 10000, "Minimum planted amount to become a Citizen (in Seeds)", high_impact);  // min planted 200 Seeds
  confwithdesc(name("cit.tx"), 10, "Minimum transactions to become a Citizen", high_impact);              // min 10 transactions
  confwithdesc(name("cit.referred"), 3, "Minimum number of referrals made to become a Citizen", high_impact);         // min referred 3 other users
  confwithdesc(name("cit.ref.res"), 1, "Minimum residents or citizens referred to become a Citizen", high_impact);          // min referred 1 resident or citizen
  confwithdesc(name("cit.rep.sc"), 50, "Minimum reputation score to become a Citizen (not points)", high_impact);          // min reputation score (not points)
  confwithdesc(name("cit.age"), 59 * 24 * 60 * 60, "Minimum account age to become a Citizen (in seconds)", high_impact);  // min account age 2 cycles 


  // =====================================
  // vouch rewards - community building points
  // =====================================

  // community buiding points for referrer when user becomes resident
  confwithdesc(name("vou.cbp1.ind"), 1, "Community buiding points for referrer when user becomes resident", high_impact);

  // community buiding points for referrer when user becomes citizen
  confwithdesc(name("vou.cbp2.ind"), 1, "Community buiding points for referrer when user becomes citizen", high_impact);

  // community buiding points for referrer when user becomes citizen
  confwithdesc(name("ref.org1.cbp1"), 4, "Community buiding points for referrer when user they invited has a reputable org - when org becomes reputable", high_impact);

  // community buiding points for Local Gathering
  confwithdesc(name("cbp.loc.meet"), 1, "Local Gathering - When you’re with a Citizen and scan their QR code. One award per lunar cycle.", high_impact);
  
  // community buiding points for Local Exchange
  confwithdesc(name("cbp.loc.exch"), 1, "Local Exchange - When you buy from an organisation in your Bioregion. One award per lunar cycle.", high_impact);

  // community buiding points for Support Regenerative Org.
  confwithdesc(name("cbp.buy.rgn"), 2, "When you buy from a Regenerative Organisation. One award per lunar cycle.", high_impact);

  // community buiding points for ATM Market Making
  confwithdesc(name("cbp.atm"), 1, "ATM Market Making: When you provide an offer in the SEEDS ATM and another member takes your offer.", high_impact);

  // community buiding points for org when user becomes resident
  confwithdesc(name("refcbp1.org"), 2, "Community buiding points for org when user becomes citizen", high_impact);

  // community buiding points for org when user becomes citizen
  confwithdesc(name("refcbp2.org"), 2, "Community buiding points for org when user becomes citizen", high_impact);

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

  // reward for individual referrer when user becomes resident  
  confwithdesc(name("refrwd1.ind"), 750 * 10000, "Reward for individual referrer when user becomes resident", high_impact); // 1000 SEEDS

  // reward for individual referrer when user becomes citizen  
  confwithdesc(name("refrwd2.ind"), 1250 * 10000, "Reward for individual referrer when user becomes citizen", high_impact); // 1500 SEEDS

  // reward for org when user becomes resident
  confwithdesc(name("refrwd1.org"), 640 * 10000, "Reward for org when user becomes resident", high_impact);  // 800 SEEDS

  // reward for org when user becomes citizen
  confwithdesc(name("refrwd2.org"), 960 * 10000, "Reward for org when user becomes citizen", high_impact); // 1200 SEEDS

  // reward for ambassador of referring org when user becomes resident
  confwithdesc(name("refrwd1.amb"), 150 * 10000, "Reward for ambassador of referring org when user becomes resident", high_impact);  // 200 SEEDS

  // reward for abmassador of referring org when user becomes citizen
  confwithdesc(name("refrwd2.amb"), 250 * 10000, "Reward for abmassador of referring org when user becomes citizen", high_impact);  // 300 SEEDS

  // Maximum number of points a user can gain from others vouching for them
  confwithdesc(name("maxvouch"), 50, "Maximum number of points a user can gain from others vouching for them", high_impact);

  // vouch base reward resident
  confwithdesc(name("res.vouch"), 10, "Vouch base reward resident", high_impact);
  
  // vouch base reward citizen
  confwithdesc(name("cit.vouch"), 20, "Vouch base reward citizen", high_impact);


  // =====================================
  // forum 
  // =====================================
  confwithdesc(name("forum.maxp"), 100000, "Max points the user can give in voting", high_impact);
  confwithdesc(name("forum.vpb"), 70000, "Vote Base Points", high_impact);
  confwithdesc(name("forum.cutoff"), 280000, "Minimum value to start the decay", high_impact);
  confwithdesc(name("forum.cutzro"), 5000, "Minimum value to set vote power to zero", high_impact);
  confwithdesc(name("forum.dp"), 9500, "Depreciation multiplier (four decimal precision)", high_impact);
  confwithdesc(name("forum.dps"), 5, "Depreciation frequency (in days)", high_impact);


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
