#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/cspoints_table.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <vector>
#include <cmath>

using namespace eosio;
using namespace utils;
using std::string;

CONTRACT proposals : public contract {
  public:
      using contract::contract;
      proposals(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          props(receiver, receiver.value),
          voice(receiver, receiver.value),
          lastprops(receiver, receiver.value),
          cycle(receiver, receiver.value),
          participants(receiver, receiver.value),
          minstake(receiver, receiver.value),
          actives(receiver, receiver.value),
          cyclestats(receiver, receiver.value),
          users(contracts::accounts, contracts::accounts.value)
          {}

      ACTION reset();

      ACTION create(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund);
      
      ACTION createx(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund, std::vector<uint64_t> pay_percentages);

      ACTION createinvite(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund, asset max_amount_per_invite, asset planted, asset reward);

      ACTION cancel(uint64_t id);

      ACTION update(uint64_t id, string title, string summary, string description, string image, string url);
      
      ACTION updatex(uint64_t id, string title, string summary, string description, string image, string url, std::vector<uint64_t> pay_percentages);

      ACTION fixdesc(uint64_t id, string description); // temp for fixing description
      ACTION applyfixprop(); // temp for fixing description
      ACTION backfixprop(); // revert fixing description
      
      ACTION stake(name from, name to, asset quantity, string memo);

      ACTION addvoice(name user, uint64_t amount);

      ACTION changetrust(name user, bool trust);

      ACTION favour(name user, uint64_t id, uint64_t amount);

      ACTION against(name user, uint64_t id, uint64_t amount);

      ACTION neutral(name user, uint64_t id);

      ACTION voteonbehalf(name voter, uint64_t id, uint64_t amount, name option);

      ACTION erasepartpts(uint64_t active_proposals);

      ACTION onperiod();

      ACTION evalproposal(uint64_t proposal_id, uint64_t prop_cycle);

      ACTION updatevoices();

      ACTION updatevoice(uint64_t start);

      ACTION checkstake(uint64_t prop_id);

      ACTION addactive(name account);

      ACTION calcvotepow();

      ACTION decayvoices();

      ACTION decayvoice(uint64_t start, uint64_t chunksize);

      ACTION testquorum(uint64_t total_proposals);

      ACTION testvdecay(uint64_t timestamp);

      ACTION testsetvoice(name user, uint64_t amount);
      ACTION initsz();

      ACTION initnumprop();

      ACTION delegate(name delegator, name delegatee, name scope);

      ACTION mimicvote(name delegatee, name delegator, name scope, uint64_t proposal_id, double percentage_used, name option, uint64_t chunksize);

      ACTION undelegate(name delegator, name scope);

      ACTION addcampaign(uint64_t proposal_id, uint64_t campaign_id);

      ACTION checkprop(uint64_t proposal_id, string message);

      ACTION doneprop(uint64_t proposal_id);


      ACTION cleanmig ();
      ACTION testpropquor(uint64_t current_cycle, uint64_t prop_id);

      ACTION testperiod ();
      ACTION testevalprop(uint64_t proposal_id, uint64_t prop_cycle);

      ACTION reevalprop (uint64_t proposal_id, uint64_t prop_cycle);

      ACTION testalliance(uint64_t id, name creator, asset quantity, asset current_payout, name status, name stage, name campaign_type);
      ACTION migalliances(uint64_t start, uint64_t chunksize);

  private:
      symbol seeds_symbol = symbol("SEEDS", 4);
      name trust = "trust"_n;
      name distrust = "distrust"_n;
      name abstain = "abstain"_n;
      name prop_active_size = "prop.act.sz"_n;
      name user_active_size = "user.act.sz"_n; 
      name cycle_vote_power_size = "votepow.sz"_n; 
      name linear_payout = "linear"_n;
      name stepped_payout = "step"_n;

      name status_open = name("open");        // 1 - open: can be cancelled, edited
      name status_evaluate = name("evaluate");
      name status_passed = name("passed");
      name status_rejected = name("rejected");

      // stages
      name stage_staged = name("staged"); // 1 staged: can be cancelled, edited
      name stage_active = name("active"); // 2 active: can be voted on, can't be edited; open or evaluate status
      name stage_done = name("done");     // 3 done: can't be edited or voted on

      std::vector<uint64_t> default_step_distribution = {
        25,  // initial payout
        25,  // cycle 1
        25,  // cycle 2
        25  // cycle 3
      };

      name alliance_type = "alliance"_n;
      name campaign_type = "campaign"_n;
      name referendum_type = "referendum"_n;

      name campaign_invite_type = "cmp.invite"_n;
      name campaign_funding_type = "cmp.funding"_n;
      name milestone_type = "milestone"_n;

      std::vector<name> scopes = {
        alliance_type,
        get_self(),
        milestone_type,
        referendum_type,
      };

      void update_cycle();
      void update_voicedecay();
      uint64_t get_cycle_period_sec();
      uint64_t get_voice_decay_period_sec();
      bool is_enough_stake(asset staked, asset quantity, name fund);
      uint64_t min_stake(asset quantity, name fund);
      uint64_t cap_stake(name fund);
      void update_min_stake(uint64_t prop_id);

      void check_user(name account);
      void check_citizen(name account);
      void check_resident(name account, bool org_allowed);
      void deposit(asset quantity);
      void withdraw(name account, asset quantity, name sender, string memo);
      void refund_staked(name beneficiary, asset quantity);
      void send_to_escrow(name fromfund, name recipient, asset quantity, string memo);
      void burn(asset quantity);
      void update_voice_table();
      void vote_aux(name voter, uint64_t id, uint64_t amount, name option, bool is_new, bool is_delegated);
      bool revert_vote (name voter, uint64_t id);
      void change_rep(name beneficiary, bool passed);
      uint64_t get_size(name id);
      void size_change(name id, int64_t delta);
      void size_set(name id, int64_t value);

      uint64_t get_quorum(uint64_t total_proposals);
      void recover_voice(name account);
      void demote_citizen(name account);
      uint64_t calculate_decay(uint64_t voice);
      name get_type (const name & fund);
      double voice_change (name user, uint64_t amount, bool reduce, name scope);
      void set_voice (name user, uint64_t amount, name scope);
      void erase_voice (name user);
      void check_percentages(std::vector<uint64_t> pay_percentages);
      asset get_payout_amount(std::vector<uint64_t> pay_percentages, uint64_t age, asset total_amount, asset current_payout);
      void check_voice_scope(name scope);
      bool is_trust_delegated(name account, name scope);
      void send_mimic_delegatee_vote(name delegatee, name scope, uint64_t proposal_id, double percentage_used, name option);
      uint64_t active_cutoff_date();
      bool is_active(name account, uint64_t cutoff_date);
      void send_vote_on_behalf(name voter, uint64_t id, uint64_t amount, name option);

      void increase_voice_cast(uint64_t amount, name option, name prop_type);
      uint64_t calc_quorum_base(uint64_t propcycle);
      void add_voted_proposal(uint64_t proposal_id);
      void create_aux(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, 
        name fund, name subtype, std::vector<uint64_t> pay_percentages, asset max_amount_per_invite, asset planted, asset reward);
      void send_create_invite(name origin_account, name owner, asset max_amount_per_invite, asset planted, name reward_owner, asset reward, asset total_amount, uint64_t proposal_id);
      void send_return_funds_campaign(uint64_t campaign_id);

      void send_eval_prop(uint64_t proposal_id, uint64_t prop_cycle);
      void init_cycle_new_stats();
      void update_cycle_stats_from_proposal(uint64_t proposal_id, name type, name array);
      void send_punish(name account);
      void send_update_voices();
      void send_cancel_lock(name fromfund, uint64_t campaign_id, asset quantity);
      bool check_prop_majority(uint64_t favour, uint64_t against);

      void send_test_eval_prop(uint64_t proposal_id, uint64_t prop_cycle);
      void set_support_level(uint64_t cycle, uint64_t num_proposals, uint64_t votes_cast, name type);
      void add_voice_cast(uint64_t cycle, uint64_t voice_cast, name type);
      void add_num_prop(uint64_t cycle, uint64_t num_prop, name type);
      uint64_t calc_voice_needed(uint64_t total_voice, uint64_t num_proposals);
      void check_values(string title, string summary, string description, string image, string url);

      uint64_t config_get(name key) {
        DEFINE_CONFIG_TABLE
        DEFINE_CONFIG_TABLE_MULTI_INDEX
        config_tables config(contracts::settings, contracts::settings.value);
        
        auto citr = config.find(key.value);
          if (citr == config.end()) { 
          // only create the error message string in error case for efficiency
          eosio::check(false, ("settings: the "+key.to_string()+" parameter has not been initialized").c_str());
        }
        return citr->value;
      }

      TABLE proposal_table {
          uint64_t id;
          name creator;
          name recipient;
          asset quantity;
          asset staked;
          bool executed;
          uint64_t total;
          uint64_t favour;
          uint64_t against;
          string title;
          string summary;
          string description;
          string image;
          string url;
          name status;
          name stage;
          name fund;
          uint64_t creation_date;
          std::vector<uint64_t> pay_percentages;
          uint64_t passed_cycle;
          uint32_t age;
          asset current_payout;
          name campaign_type;
          asset max_amount_per_invite;
          asset planted;
          asset reward;
          uint64_t campaign_id;

          uint64_t primary_key()const { return id; }
          uint64_t by_status()const { return status.value; }
          uint64_t by_stage()const { return stage.value; }
          uint64_t by_campaign()const { return campaign_id; }
          uint64_t by_creator()const { return creator.value; }

          uint128_t by_status_id()const { return (uint128_t(status.value) << 64) + id; }
          uint128_t by_stage_id()const { return (uint128_t(stage.value) << 64) + id; }
          uint128_t by_campaign_type_id()const { return (uint128_t(campaign_type.value) << 64) + id; }
      };

      TABLE min_stake_table {
          uint64_t prop_id;
          uint64_t min_stake;
          uint64_t primary_key()const { return prop_id; }
      };

      TABLE fix_props_table {
          uint64_t prop_id;
          string description;
          uint64_t primary_key()const { return prop_id; }
      };
      typedef eosio::multi_index<"fixprops"_n, fix_props_table> fix_props_tables;

      TABLE fixb_props_table {
          uint64_t prop_id;
          string description;
          uint64_t primary_key()const { return prop_id; }
      };
      typedef eosio::multi_index<"fixbprops"_n, fixb_props_table> fixb_props_tables;

      DEFINE_USER_TABLE

      TABLE vote_table {
          uint64_t proposal_id;
          name account;
          uint64_t amount;
          bool favour;
          uint64_t primary_key()const { return account.value; }
      };

      TABLE participant_table {
        name account;
        bool nonneutral;
        uint64_t count;
        // bool complete;
        uint64_t primary_key()const { return account.value; }
      };

      TABLE voice_table {
        name account;
        uint64_t balance;
        uint64_t primary_key()const { return account.value; }
      };

      TABLE last_proposal_table {
        name account;
        uint64_t proposal_id;
        uint64_t primary_key()const { return account.value; }
      };

      TABLE cycle_table {
        uint64_t propcycle; 
        uint64_t t_onperiod; // last time onperiod ran
        uint64_t t_voicedecay; // last time voice was decayed
      };

      TABLE active_table {
        name account;
        uint64_t timestamp;

        uint64_t primary_key()const { return account.value; }
      };

      TABLE delegate_trust_table { // scoped by proposal's category (alliance, campaign, etc)
        name delegator;
        name delegatee;
        double weight;
        uint64_t timestamp;

        uint64_t primary_key()const { return delegator.value; }
        uint64_t by_delegatee()const { return delegatee.value; }
        uint128_t by_delegatee_delegator() const { return (uint128_t(delegatee.value) << 64) + delegator.value; }
      };

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

      TABLE support_level_table {
        uint64_t propcycle; 
        
        uint64_t num_proposals;
        uint64_t total_voice_cast;
        uint64_t voice_needed;

        uint64_t primary_key()const { return propcycle; }
      };

      TABLE voted_proposals_table { // scoped by cycle
        uint64_t proposal_id;

        uint64_t primary_key()const { return proposal_id; }
      };

    typedef eosio::multi_index<"props"_n, proposal_table,
      indexed_by<"bystatus"_n,
      const_mem_fun<proposal_table, uint64_t, &proposal_table::by_status>>,
      indexed_by<"bystage"_n,
      const_mem_fun<proposal_table, uint64_t, &proposal_table::by_stage>>,
      indexed_by<"bycampaign"_n,
      const_mem_fun<proposal_table, uint64_t, &proposal_table::by_campaign>>,
      indexed_by<"bycreator"_n,
      const_mem_fun<proposal_table, uint64_t, &proposal_table::by_creator>>,
      indexed_by<"bystatusid"_n,
      const_mem_fun<proposal_table, uint128_t, &proposal_table::by_status_id>>,
      indexed_by<"bystageid"_n,
      const_mem_fun<proposal_table, uint128_t, &proposal_table::by_stage_id>>,
      indexed_by<"bycmptypeid"_n,
      const_mem_fun<proposal_table, uint128_t, &proposal_table::by_campaign_type_id>>
    > proposal_tables;
    
    typedef eosio::multi_index<"votes"_n, vote_table> votes_tables;
    typedef eosio::multi_index<"participants"_n, participant_table> participant_tables;
    typedef eosio::multi_index<"users"_n, user_table> user_tables;
    typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;
    typedef eosio::multi_index<"lastprops"_n, last_proposal_table> last_proposal_tables;
    typedef singleton<"cycle"_n, cycle_table> cycle_tables;
    typedef eosio::multi_index<"cycle"_n, cycle_table> dump_for_cycle;
    typedef eosio::multi_index<"minstake"_n, min_stake_table> min_stake_tables;
    typedef eosio::multi_index<"actives"_n, active_table> active_tables;
    typedef eosio::multi_index<"deltrusts"_n, delegate_trust_table,
      indexed_by<"bydelegatee"_n,
      const_mem_fun<delegate_trust_table, uint64_t, &delegate_trust_table::by_delegatee>>,
      indexed_by<"byddelegator"_n,
      const_mem_fun<delegate_trust_table, uint128_t, &delegate_trust_table::by_delegatee_delegator>>
    > delegate_trust_tables;
    typedef eosio::multi_index<"cyclestats"_n, cycle_stats_table> cycle_stats_tables;
    typedef eosio::multi_index<"cycvotedprps"_n, voted_proposals_table> voted_proposals_tables;

    typedef eosio::multi_index<"support"_n, support_level_table> support_level_tables;

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX

    proposal_tables props;
    participant_tables participants;
    user_tables users;
    voice_tables voice;
    last_proposal_tables lastprops;
    cycle_tables cycle;
    min_stake_tables minstake;
    active_tables actives;
    cycle_stats_tables cyclestats;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<proposals>(name(receiver), name(code), &proposals::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(proposals, (reset)(create)(createx)(createinvite)(update)(updatex)(addvoice)(changetrust)(favour)(against)
        (neutral)(erasepartpts)(checkstake)(onperiod)(evalproposal)(decayvoice)(cancel)(updatevoices)(updatevoice)(decayvoices)
        (addactive)(testvdecay)(initsz)(testquorum)(initnumprop)
        (testsetvoice)(delegate)(mimicvote)(undelegate)(voteonbehalf)
        (calcvotepow)(addcampaign)(checkprop)(doneprop)
        (testperiod)(testevalprop)
        (cleanmig)(testpropquor)
        (reevalprop)
        (testalliance)(migalliances)
        (fixdesc)(applyfixprop)(backfixprop)
        )
      }
  }
}
