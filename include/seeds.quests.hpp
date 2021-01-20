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


#define QUEST_FIXED "quest_fixed"
#define QUEST_VARIABLE "quest_variable"
#define MILESTONE_FIXED "milestone_fixed"
#define MILESTONE_VARIABLE "milestone_variable"

using namespace eosio;
using namespace utils;
using std::string;

CONTRACT quests : public contract {
  
  public:
    using contract::contract;
    
    quests(name receiver, name code, datastream<const char*> ds)
    : contract(receiver, code, ds)
    {}

    DECLARE_DOCUMENT_GRAPH(quests)

    ACTION reset();

    ACTION stake(name from, name to, asset quantity, string memo);

    ACTION addquest(name creator, asset amount, string title, string description, name fund);

    // ACTION addmilestone(checksum256 quest_hash, string title, string description, double payout_percentage);


  private:

    const name quest_status_open = name("open");
    const name quest_status_passed = name("passed");
    const name quest_status_rejected = name("rejected");

    const name milestone_status_unassigned = name("unassigned");

    const name stage_staged = name("staged"); // 1 staged: can be cancelled, edited
    const name stage_active = name("active"); // 2 active: can be voted on, can't be edited
    const name stage_done = name("done");     // 3 done: can't be edited or voted on


    void check_auth_creator(name creator);

    hypha::DocumentGraph m_documentGraph = hypha::DocumentGraph(get_self());

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<quests>(name(receiver), name(code), &quests::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(quests, (reset)
          (addquest)
        )
      }
  }
}
