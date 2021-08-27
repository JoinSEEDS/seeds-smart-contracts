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
#include <utility>
#include <cmath>

#include <graph_common.hpp>
#include <document_graph/content.hpp>
#include <document_graph/document.hpp>
#include <document_graph/edge.hpp>
#include <document_graph/util.hpp>
#include <document_graph/content_wrapper.hpp>
#include <document_graph/document_graph.hpp>


using namespace eosio;
using namespace utils;
using std::string;

CONTRACT quests : public contract {
  
  public:
    using contract::contract;
    
    quests(name receiver, name code, datastream<const char*> ds)
    : contract(receiver, code, ds),
      users(contracts::accounts, contracts::accounts.value),
      sizes(contracts::proposals, contracts::proposals.value)
      {}

    DECLARE_DOCUMENT_GRAPH(quests)

    ACTION reset();

    ACTION stake(name from, name to, asset quantity, string memo);

    ACTION withdraw (name beneficiary, asset quantity);

    ACTION addquest(name creator, asset amount, string title, string description, name fund, int64_t duration);

    ACTION addmilestone(checksum256 quest_hash, string title, string description, uint64_t payout_percentage);

    ACTION delmilestone(checksum256 milestone_hash);

    ACTION activate(checksum256 quest_hash);

    ACTION propactivate(checksum256 quest_hash);

    ACTION notactivate(checksum256 quest_hash);

    ACTION delquest(checksum256 quest_hash);

    ACTION apply(checksum256 quest_hash, name applicant, string description);

    ACTION accptapplcnt(checksum256 applicant_hash);

    ACTION rejctapplcnt (checksum256 applicant_hash);

    ACTION accptquest(checksum256 quest_hash);

    ACTION mcomplete(checksum256 milestone_hash, string url_documentation, string description);

    ACTION accptmilstne(checksum256 milestone_hash);

    ACTION propaccptmil(checksum256 milestone_hash);

    ACTION payoutmilstn(checksum256 milestone_hash);

    ACTION rejctmilstne(checksum256 milestone_hash);

    ACTION expirequest(checksum256 quest_hash);

    ACTION expireappl(checksum256 maker_hash);

    ACTION cancelappl(checksum256 maker_hash);

    ACTION retractappl(checksum256 applicant_hash);

    ACTION quitapplcnt(checksum256 applicant_hash);

    // ACTION onperiod(name just_one);

    ACTION evalprop(checksum256 proposal_hash);

    ACTION favour(name voter, checksum256 proposal_hash, int64_t amount);

    ACTION against(name voter, checksum256 proposal_hash, int64_t amount);

    ACTION rateapplcnt(checksum256 maker_hash, name opinion);

    ACTION ratequest(checksum256 quest_hash, name opinion);


  private:

    const name quest_status_open = name("open");
    const name quest_status_finished = name("finished");
    const name quest_status_expired = name("expired");

    const name milestone_status_not_completed = name("notcompleted");
    const name milestone_status_completed = name("completed");
    const name milestone_status_finished = name("finished");

    const name quest_stage_staged = name("staged");
    const name quest_stage_proposed = name("proposed");
    const name quest_stage_active = name("active");
    const name quest_stage_done = name("done");

    const name applicant_status_pending = name("pending");
    const name applicant_status_accepted = name("accepted");
    const name applicant_status_rejected = name("rejected");
    const name applicant_status_confirmed = name("confirmed");
    const name applicant_status_expired = name("expired");
    const name applicant_status_cancel = name("cancel");
    const name applicant_status_quitted = name("quitted");

    const name proposal_type_quest = name("prop.quest");
    const name proposal_type_milestone = name("prop.milstne");
    const name proposal_type_applicant = name("prop.applcnt");
    const name proposal_type_maker = name("prop.maker");

    const name proposal_status_open = name("open");
    const name proposal_status_passed = name("passed");
    const name proposal_status_rejected = name("rejected");

    const name proposal_stage_staged = name("staged");
    const name proposal_stage_active = name("active");
    const name proposal_stage_done = name("done");

    const name trust = name("trust");
    const name distrust = name("distrust");
    const name abstain = name("abstain");

    const name user_active_size = name("user.act.sz");
    const name active_proposals = name("active.p.sz");

    const name like = name("like");
    const name dislike = name("dislike");


    void check_type(hypha::Document & node_doc, const name & type);
    void check_quest_status_stage(hypha::ContentWrapper & cw, const name & status, const name & stage, const string & error_msg);
    void check_quest_status_stage(const checksum256 & quest_hash, const name & status, const name & stage, const string & error_msg);
    void validate_milestones(const checksum256 & quest_hash);
    void update_balance(hypha::Document & balance_doc, asset & quantity, const bool & substract);
    void add_balance(hypha::Document & balance_doc, asset & quantity);
    void sub_balance(hypha::Document & balance_doc, asset & quantity);
    void check_user(name & account);
    void update_milestone_status(hypha::Document * milestone_v_doc, const name & new_status, const name & check_status);
    void send_to_escrow(const name & fromfund, const name & recipient, asset & quantity, const string & memo);
    void update_node(hypha::Document * node_doc, const string & content_group_label, const std::vector<hypha::Content> & new_contents);
    void propose_aux(const checksum256 & node_hash, const name & quest_owner, const name & passed_action, const name & rejected_action);
    void proposal_quest_aux(hypha::Document & quest_doc);
    void vote_aux(name & voter, const checksum256 & proposal_hash, int64_t & amount, const name & option);
    void check_auth(name & creator, name & fund);
    void accept_milestone(hypha::Document & milestone_doc, hypha::Document & milestone_v_doc);
    
    int64_t active_cutoff_date();
    bool is_active(hypha::ContentWrapper & account_info_v_cw, int64_t & cutoff_date);
    hypha::Document get_doc_from_edge (const checksum256 & node_hash, const name & edge_name);
    name get_proposal_type(name & node_type);
    hypha::Document get_root_node();
    hypha::Document get_account_infos_node();
    hypha::Document get_proposals_node ();
    hypha::Document get_variable_node_or_fail(hypha::Document & fixed_node);
    hypha::Document get_account_info(name & account, const bool & create_if_not_exists);
    hypha::Document get_quest_node_from_milestone(hypha::Document & milestone_doc);
    bool edge_exists(const checksum256 & from_node_hash, const name & edge_name);
    uint64_t get_quorum();
    uint64_t get_size(name id);
    bool is_voted_quest(hypha::Document & quest_doc);
    bool check_auth_create_proposal(name & creator, name & fund);

    hypha::DocumentGraph m_documentGraph = hypha::DocumentGraph(get_self());


    DEFINE_USER_TABLE
    DEFINE_USER_TABLE_MULTI_INDEX

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX


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

    user_tables users;
    size_tables sizes;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<quests>(name(receiver), name(code), &quests::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(quests, (reset)
          (withdraw)
          (addquest)(activate)(propactivate)(notactivate)(delquest)(apply)(accptapplcnt)(rejctapplcnt)(accptquest)
          (addmilestone)(delmilestone)(mcomplete)(accptmilstne)(propaccptmil)(payoutmilstn)(rejctmilstne)
          (expirequest)(expireappl)(cancelappl)(retractappl)(quitapplcnt)
          (evalprop)(favour)(against)
          (rateapplcnt)(ratequest)
        )
      }
  }
}
