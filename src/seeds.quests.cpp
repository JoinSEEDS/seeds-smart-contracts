#include <seeds.quests.hpp>

#include "document_graph/content.cpp"
#include "document_graph/document.cpp"
#include "document_graph/edge.cpp"
#include "document_graph/util.cpp"
#include "document_graph/content_wrapper.cpp"
#include "document_graph/document_graph.cpp"


ACTION quests::reset () {

  require_auth(get_self());

  document_table d_t(_self, _self.value);
  auto ditr = d_t.begin();
  while (ditr != d_t.end()) {
    ditr = d_t.erase(ditr);
  }

  edge_table e_t(_self, _self.value);
  auto eitr = e_t.begin();
  while (eitr != e_t.end()) {
    eitr = e_t.erase(eitr);
  }

}

ACTION quests::stake (name from, name to, asset quantity, string memo) {}

ACTION quests::addquest (name creator, asset amount, string title, string description, name fund) {

  require_auth(creator);
  utils::check_asset(amount);

  if (fund != creator) {
    check(fund == bankaccts::campaigns, "invalid fund, fund must be either the quest creator or " + bankaccts::campaigns.to_string());
  } 

  // check for some seeds staked?

  hypha::ContentGroups questCgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, QUEST_FIXED),
      hypha::Content(TYPE, graph::QUEST),
      hypha::Content(TITLE, title),
      hypha::Content(DESCRIPTION, description),
      hypha::Content(AMOUNT, amount),
      hypha::Content(FUND, fund)
    }
  };

  hypha::ContentGroups questInfoCgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, QUEST_VARIABLE),
      hypha::Content(STATUS, quest_status_open),
      hypha::Content(STAGE, stage_staged)
    }
  };

  hypha::Document questDoc(get_self(), creator, std::move(questCgs));
  hypha::Document questInfoDoc(get_self(), creator, std::move(questInfoCgs));

  hypha::Edge::write(get_self(), creator, questDoc.getHash(), questInfoDoc.getHash(), graph::OWNS);

}