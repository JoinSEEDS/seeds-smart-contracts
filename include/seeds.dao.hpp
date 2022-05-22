#pragma once

#include <contracts.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/user_table.hpp>
#include <tables/proposals_table.hpp>
#include <tables/size_table.hpp>
#include <tables/cspoints_table.hpp>
#include <tables/organization_table.hpp>
#include <tables/dho_share_table.hpp>
#include <tables/moon_phases_table.hpp>
#include <cmath>

using namespace eosio;
using std::string;
using std::make_tuple;
using std::distance;


CONTRACT dao : public contract {
  public:
      using contract::contract;
      dao(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          sizes(receiver, receiver.value),
          config(contracts::settings, contracts::settings.value)
          {}

      name alliance_scope = "alliance"_n;
      name campaign_scope = "campaign"_n;
      name milestone_scope = "milestone"_n;
      name referendums_scope = "referendum"_n;
      name dhos_scope = "dhos"_n;

      std::vector<name> scopes = {
        alliance_scope,
        campaign_scope,
        milestone_scope,
        referendums_scope,
        dhos_scope
      };

      name alliance_fund = "alliance"_n;
      name campaign_fund = "campaign"_n;
      name milestone_fund = "milestone"_n;

      std::vector<name> fund_types = {
        alliance_fund,
        campaign_fund,
        milestone_fund
      };

      const name prop_active_size = "prop.act.sz"_n;
      const name user_active_size = "user.act.sz"_n;
      const name dhos_vote_size = "dho.vote.sz"_n; 
      const name linear_payout = "linear"_n;
      const name stepped_payout = "step"_n;

      typedef struct dhovote {
        name dho;
        uint64_t points;
      } DhoVote;


      ACTION reset();

      ACTION initcycle(const uint64_t & cycle_id);

      ACTION initsz();

      ACTION stake(const name & from, const name & to, const asset & quantity, const string & memo);

      ACTION create(std::map<std::string, VariantValue> & args);

      ACTION update(std::map<std::string, VariantValue> & args);

      ACTION cancel(std::map<std::string, VariantValue> & args);

      ACTION callback(std::map<std::string, VariantValue> & args);

      ACTION onperiod();

      ACTION evaluate(const uint64_t & proposal_id, const uint64_t & propcycle);

      ACTION favour(const name & voter, const uint64_t & proposal_id, const uint64_t & amount);

      ACTION against(const name & voter, const uint64_t & proposal_id, const uint64_t & amount);

      ACTION neutral(const name & voter, const uint64_t & proposal_id);

      ACTION revertvote(const name & voter, const uint64_t & proposal_id);

      ACTION changetrust(const name & user, const bool & trust);

      ACTION addactive(const name & account);

      ACTION delegate(const name & delegator, const name & delegatee, const name & scope);

      ACTION undelegate(const name & delegator, const name & scope);

      ACTION mimicvote(const name & delegatee, const name & delegator, const name & scope, const uint64_t & proposal_id, const double & percentage_used, const name & option, const uint64_t chunksize);

      ACTION voteonbehalf(const name & voter, const uint64_t & proposal_id, const uint64_t & amount, const name & option);

      ACTION decayvoices();

      ACTION decayvoice(const uint64_t & start, const uint64_t & chunksize);

      ACTION mimicrevert(const name & delegatee, const uint64_t & delegator, const name & scope, const uint64_t & proposal_id, const uint64_t & chunksize);

      ACTION updatevoices();

      ACTION updatevoice(const uint64_t & start, const name & scope);

      ACTION erasepartpts(const uint64_t & active_proposals);

      ACTION createdho(const name & organization);

      ACTION removedho(const name & organization);

      ACTION removedhovts(const name & organization, const uint64_t & start, const uint64_t & chunksize, const bool & remove_size);

      ACTION votedhos(const name & account, std::vector<DhoVote> votes);

      ACTION dhomimicvote(const name & delegatee, const uint64_t & start, std::vector<DhoVote> votes, const uint64_t & chunksize);

      ACTION dhocleanvts();

      ACTION dhocleanvote(const uint64_t & cutoff, const uint64_t & chunksize);

      ACTION dhocalcdists();

      ACTION deletescope(const uint64_t & start, const name & scope); 

      ACTION addvoice(const uint64_t & start, const name & scope);


      ACTION testsetvoice(const name & account, const uint64_t & amount);



      name get_fund_type(const name & fund);

      uint64_t calc_quorum_base(const uint64_t & propcycle);
      void update_cycle_stats_from_proposal(const uint64_t & proposal_id, const name & type, const name & array);


      template <typename... T>
      void send_deferred_transaction(const permission_level & permission, const name & contract, const name & action, const std::tuple<T...> & data);

      template <typename... T>
      void send_inline_action(const permission_level & permission, const name & contract, const name & action, const std::tuple<T...> & data);


      DEFINE_PROPOSAL_TABLE
      DEFINE_PROPOSAL_TABLE_MULTI_INDEX

      DEFINE_PROPOSAL_AUXILIARY_TABLE
      DEFINE_PROPOSAL_AUXILIARY_TABLE_MULTI_INDEX

      DEFINE_CONFIG_TABLE
      DEFINE_CONFIG_TABLE_MULTI_INDEX
      DEFINE_CONFIG_GET

      DEFINE_CONFIG_FLOAT_TABLE
      DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

      DEFINE_SIZE_TABLE
      DEFINE_SIZE_TABLE_MULTI_INDEX
      DEFINE_SIZE_CHANGE
      DEFINE_SIZE_SET
      DEFINE_SIZE_GET

      DEFINE_USER_TABLE
      DEFINE_USER_TABLE_MULTI_INDEX

      DEFINE_CS_POINTS_TABLE
      DEFINE_CS_POINTS_TABLE_MULTI_INDEX

      DEFINE_ORGANIZATION_TABLE
      DEFINE_ORGANIZATION_TABLE_MULTI_INDEX

      DEFINE_MOON_PHASES_TABLE
      DEFINE_MOON_PHASES_TABLE_MULTI_INDEX

      TABLE deferred_id_table {
        uint64_t id;
      };
      typedef singleton<"deferredids"_n, deferred_id_table> deferred_id_tables;
      typedef eosio::multi_index<"deferredids"_n, deferred_id_table> dump_for_deferred_id;

      TABLE cycle_table {
        uint64_t propcycle; 
        uint64_t t_onperiod; // last time onperiod ran
        uint64_t t_voicedecay; // last time voice was decayed
      };
      typedef singleton<"cycle"_n, cycle_table> cycle_tables;
      typedef eosio::multi_index<"cycle"_n, cycle_table> dump_for_cycle;

      TABLE vote_table {
        uint64_t proposal_id;
        name account;
        uint64_t amount;
        bool favour;

        uint64_t primary_key()const { return account.value; }
      };
      typedef eosio::multi_index<"votes"_n, vote_table> votes_tables;

      TABLE voice_table {
        name account;
        uint64_t balance;

        uint64_t primary_key()const { return account.value; }
      };
      typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;

      TABLE active_table {
        name account;
        uint64_t timestamp;

        uint64_t primary_key()const { return account.value; }
      };
      typedef eosio::multi_index<"actives"_n, active_table> active_tables;

      TABLE participant_table {
        name account;
        bool nonneutral;
        uint64_t count;

        uint64_t primary_key()const { return account.value; }
      };
      typedef eosio::multi_index<"participants"_n, participant_table> participant_tables;

      TABLE delegate_trust_table { // scoped by proposal's scope (alliance, campaign, etc)
        name delegator;
        name delegatee;
        double weight;
        uint64_t timestamp;

        uint64_t primary_key()const { return delegator.value; }
        uint64_t by_delegatee()const { return delegatee.value; }
        uint128_t by_delegatee_delegator() const { return (uint128_t(delegatee.value) << 64) + delegator.value; }
      };
      typedef eosio::multi_index<"deltrusts"_n, delegate_trust_table,
        indexed_by<"bydelegatee"_n,
        const_mem_fun<delegate_trust_table, uint64_t, &delegate_trust_table::by_delegatee>>,
        indexed_by<"byddelegator"_n,
        const_mem_fun<delegate_trust_table, uint128_t, &delegate_trust_table::by_delegatee_delegator>>
      > delegate_trust_tables;

      TABLE voted_proposals_table { // scoped by cycle
        uint64_t proposal_id;

        uint64_t primary_key()const { return proposal_id; }
      };
      typedef eosio::multi_index<"cycvotedprps"_n, voted_proposals_table> voted_proposals_tables;

      TABLE cycle_stats_table {
        uint64_t propcycle; 
        
        uint64_t start_time; 
        uint64_t end_time; 
        uint64_t num_proposals;           // unused -> see support_level_table, scoped by type
        uint64_t num_votes;
        uint64_t total_voice_cast;
        uint64_t total_favour;
        uint64_t total_against; 
        uint64_t total_citizens;
        uint64_t quorum_vote_base;        // unused -> see support_level_table scoped by type
        uint64_t quorum_votes_needed;     // unused -> see support_level_table scoped by type
        uint64_t total_eligible_voters;
        float unity_needed;

        std::vector<uint64_t> active_props;
        std::vector<uint64_t> eval_props;

        uint64_t primary_key()const { return propcycle; }
      };
      typedef eosio::multi_index<"cyclestats"_n, cycle_stats_table> cycle_stats_tables;

      TABLE support_level_table {
        uint64_t propcycle; 
        
        uint64_t num_proposals;
        uint64_t total_voice_cast;
        uint64_t voice_needed;

        uint64_t primary_key()const { return propcycle; }
      };
      typedef eosio::multi_index<"support"_n, support_level_table> support_level_tables;

      TABLE last_proposal_table {
        name account;
        uint64_t proposal_id;

        uint64_t primary_key()const { return account.value; }
      };
      typedef eosio::multi_index<"lastprops"_n, last_proposal_table> last_proposal_tables;


      TABLE min_stake_table {
          uint64_t prop_id;
          uint64_t min_stake;
          
          uint64_t primary_key()const { return prop_id; }
      };
      typedef eosio::multi_index<"minstake"_n, min_stake_table> min_stake_tables;

      TABLE dho_table {
        name org_name;
        uint64_t points;

        uint64_t primary_key () const { return org_name.value; }
        uint128_t by_points_name () const { return (uint128_t(points) << 64) + org_name.value; }
      };
      typedef eosio::multi_index<"dhos"_n, dho_table,
        indexed_by<"bypointsname"_n,
        const_mem_fun<dho_table, uint128_t, &dho_table::by_points_name>>
      > dho_tables;

      TABLE dho_vote_table {
        uint64_t vote_id;
        name account;
        name dho;
        uint64_t points;
        uint64_t timestamp;

        uint64_t primary_key () const { return vote_id; }
        uint128_t by_timestamp_id () const { return (uint128_t(timestamp) << 64) + vote_id; }
        uint128_t by_account_id () const { return (uint128_t(account.value) << 64) + vote_id; }
        uint128_t by_dho_id () const { return (uint128_t(dho.value) << 64) + vote_id; }
      };
      typedef eosio::multi_index<"dhovotes"_n, dho_vote_table,
        indexed_by<"bytimeid"_n,
        const_mem_fun<dho_vote_table, uint128_t, &dho_vote_table::by_timestamp_id>>,
        indexed_by<"byacctid"_n,
        const_mem_fun<dho_vote_table, uint128_t, &dho_vote_table::by_account_id>>,
        indexed_by<"bydhoid"_n,
        const_mem_fun<dho_vote_table, uint128_t, &dho_vote_table::by_dho_id>>
      > dho_vote_tables;

      DEFINE_DHO_SHARE_TABLE
      DEFINE_DHO_SHARE_TABLE_MULTI_INDEX
      

      config_tables config;
      size_tables sizes;
    
    void check_citizen(const name & account);
    void check_attributes(const std::map<std::string, VariantValue> & args);

  private:

    void set_voice(const name & user, const uint64_t & amount, const name & scope);
    double voice_change(const name & user, const uint64_t & amount, const bool & reduce, const name & scope);
    void erase_voice(const name & user);
    void recover_voice(const name & account);
    uint64_t calculate_decay(const uint64_t & voice_amount);
    bool is_trust_delegated(const name & account, const name & scope);
    void send_mimic_delegatee_vote(const name & delegatee, const name & scope, const uint64_t & proposal_id, const double & percentage_used, const name & option);
    void add_voted_proposal(const uint64_t & proposal_id);
    void increase_voice_cast(const uint64_t & amount, const name & option, const name & prop_type);
    void add_voice_cast(const uint64_t & cycle, const uint64_t & voice_cast, const name & type);

    // void check_citizen(const name & account);
    void vote_aux(const name & voter, const uint64_t & referendum_id, const uint64_t & amount, const name & option, const bool & is_delegated);
    bool revert_vote(const name & voter, const uint64_t & referendum_id);
    // void check_attributes(const std::map<std::string, VariantValue> & args);
    uint64_t active_cutoff_date();
    bool has_delegates(const name & voter, const name & scope);
    bool is_active(const name & account, const uint64_t & cutoff_date);
    
    void init_cycle_new_stats();
    uint64_t calc_voice_needed(const uint64_t & total_voice, const uint64_t & num_proposals);
    double get_quorum(uint64_t total_proposals);
    uint64_t get_new_moon(uint64_t timestamp);

};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<dao>(name(receiver), name(code), &dao::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(dao, 
          (reset)(initcycle)
          (create)(update)(cancel)(onperiod)(evaluate)(callback)
          (changetrust)(addactive)
          (favour)(against)(neutral)(revertvote)(voteonbehalf)
          (delegate)(undelegate)(mimicvote)(mimicrevert)
          (decayvoices)(decayvoice)
          (updatevoices)(updatevoice)
          (erasepartpts)
          (createdho)(removedho)(removedhovts)(votedhos)(dhomimicvote)(dhocleanvts)(dhocleanvote)(dhocalcdists)
          (testsetvoice)(deletescope)(addvoice)
        )
      }
  }
}
