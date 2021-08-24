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

  // create the root node
  hypha::ContentGroups root_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::ROOT),
      hypha::Content(OWNER, get_self())
    }
  };

  hypha::ContentGroups account_infos_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::ACCOUNT_INFOS),
      hypha::Content(OWNER, get_self())
    }
  };

  hypha::ContentGroups account_infos_v_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
      hypha::Content(ACCOUNT_BALANCE, asset(0, utils::seeds_symbol)),
      hypha::Content(LOCKED_BALANCE, asset(0, utils::seeds_symbol)),
      hypha::Content(OWNER, get_self())
    }
  };

  hypha::ContentGroups proposals_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::PROPOSALS),
      hypha::Content(OWNER, get_self())
    }
  };

  hypha::Document root_doc(get_self(), get_self(), std::move(root_cgs));
  hypha::Document account_infos_doc(get_self(), get_self(), std::move(account_infos_cgs));
  hypha::Document account_infos_v_doc(get_self(), get_self(), std::move(account_infos_v_cgs));
  hypha::Document proposals_doc(get_self(), get_self(), std::move(proposals_cgs));

  hypha::Edge::write(get_self(), get_self(), root_doc.getHash(), account_infos_doc.getHash(), graph::OWNS_ACCOUNT_INFOS);
  hypha::Edge::write(get_self(), get_self(), account_infos_doc.getHash(), root_doc.getHash(), graph::OWNED_BY);
  hypha::Edge::write(get_self(), get_self(), account_infos_doc.getHash(), account_infos_v_doc.getHash(), graph::VARIABLE);

  hypha::Edge::write(get_self(), get_self(), root_doc.getHash(), proposals_doc.getHash(), graph::OWNS_PROPOSALS);
  hypha::Edge::write(get_self(), get_self(), proposals_doc.getHash(), root_doc.getHash(), graph::OWNED_BY);

  get_account_info(bankaccts::campaigns, true);

}

ACTION quests::stake (name from, name to, asset quantity, string memo) {

  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
      to  ==  get_self()) {                         // to here

    utils::check_asset(quantity);

    hypha::Document account_info_doc = get_account_info(from, true);
    hypha::Document account_info_v_doc = get_variable_node_or_fail(account_info_doc);

    if (from == bankaccts::campaigns) {
      hypha::ContentWrapper account_info_v_cw = account_info_v_doc.getContentWrapper();
      asset locked_balance = account_info_v_cw.getOrFail(VARIABLE_DETAILS, LOCKED_BALANCE) -> getAs<asset>();
      update_node(&account_info_v_doc, VARIABLE_DETAILS, {
        hypha::Content(LOCKED_BALANCE, locked_balance + quantity)
      });
      print("FROM:", from, ", QUANTITY:", quantity, "\n");
    } else {
      add_balance(account_info_v_doc, quantity);
    }

    hypha::Document account_infos_doc = get_account_infos_node();
    hypha::Document account_infos_v_doc = get_variable_node_or_fail(account_infos_doc);
    add_balance(account_infos_v_doc, quantity);

  }
  
}

ACTION quests::withdraw (name beneficiary, asset quantity) {

  require_auth(beneficiary);
  utils::check_asset(quantity);

  hypha::Document account_info_doc = get_account_info(beneficiary, false);
  hypha::Document account_info_v_doc = get_variable_node_or_fail(account_info_doc);
  sub_balance(account_info_v_doc, quantity);

  token::transfer_action action{contracts::token, {get_self(), "active"_n}};
  action.send(get_self(), beneficiary, quantity, "");

  hypha::Document balances_doc = get_account_infos_node();
  hypha::Document balances_v_doc = get_variable_node_or_fail(balances_doc);
  sub_balance(balances_v_doc, quantity);

}

ACTION quests::addquest (name creator, asset amount, string title, string description, name fund, int64_t duration) {

  require_auth(creator);
  utils::check_asset(amount);
  check_user(creator);

  if (fund != creator) {
    check(fund == bankaccts::campaigns, "quests: invalid fund, fund must be either the quest creator or " + bankaccts::campaigns.to_string());
  } 

  // check for some seeds staked?

  hypha::ContentGroups quest_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::QUEST),
      hypha::Content(TITLE, title),
      hypha::Content(DESCRIPTION, description),
      hypha::Content(AMOUNT, amount),
      hypha::Content(FUND, fund),
      hypha::Content(DURATION, duration)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(OWNER, creator)
    }
  };

  hypha::Document quest_doc(get_self(), creator, std::move(quest_cgs));

  hypha::ContentGroups quest_v_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
      hypha::Content(STATUS, quest_status_open),
      hypha::Content(STAGE, quest_stage_staged),
      hypha::Content(CURRENT_PAYOUT_AMOUNT, asset(0, utils::seeds_symbol)),
      hypha::Content(UNFINISHED_MILESTONES, int64_t(0)),
      hypha::Content(STARTED_DATE, int64_t(0))
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(QUEST_HASH, quest_doc.getHash()),
      hypha::Content(OWNER, creator)
    }
  };

  hypha::Document quest_v_doc(get_self(), creator, std::move(quest_v_cgs));

  hypha::Document root_doc = get_root_node();
  hypha::Document account_info_doc = get_account_info(creator, true);

  hypha::Edge::write(get_self(), creator, quest_doc.getHash(), quest_v_doc.getHash(), graph::VARIABLE);
  hypha::Edge::write(get_self(), creator, root_doc.getHash(), quest_doc.getHash(), graph::HAS_QUEST);
  hypha::Edge::write(get_self(), creator, account_info_doc.getHash(), quest_doc.getHash(), graph::CREATE);

}

ACTION quests::addmilestone (checksum256 quest_hash, string title, string description, uint64_t payout_percentage) {

  hypha::Document quest_doc(get_self(), quest_hash);
  name creator = quest_doc.getCreator();

  check_type(quest_doc, graph::QUEST);

  require_auth(creator);
  check_user(creator);
  check_quest_status_stage(quest_hash, ""_n, quest_stage_staged, "quests: can not add milestone when quest is not staged");

  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();
  int64_t unfinished_milestones = quest_v_cw.getOrFail(VARIABLE_DETAILS, UNFINISHED_MILESTONES) -> getAs<int64_t>();

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(UNFINISHED_MILESTONES, unfinished_milestones + 1)
  });

  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  asset amount = quest_cw.getOrFail(FIXED_DETAILS, AMOUNT) -> getAs<asset>();

  asset payout_amount = asset(amount.amount * (payout_percentage / 10000.0), utils::seeds_symbol);  

  hypha::ContentGroups milestone_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::MILESTONE),
      hypha::Content(TITLE, title),
      hypha::Content(DESCRIPTION, description),
      hypha::Content(PAYOUT_PERCENTAGE, payout_percentage),
      hypha::Content(PAYOUT_AMOUNT, payout_amount)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(QUEST_HASH, quest_doc.getHash()),
      hypha::Content(OWNER, creator)
    }
  };

  hypha::Document milestone_doc(get_self(), creator, std::move(milestone_cgs));

  hypha::ContentGroups milestone_v_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
      hypha::Content(STATUS, milestone_status_not_completed),
      hypha::Content(URL_DOCUMENTATION, ""),
      hypha::Content(DESCRIPTION, ""),
      hypha::Content(IS_PAID_OUT, int64_t(0)),
      hypha::Content(DUE_DATE, int64_t(0))
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(MILESTONE_HASH, milestone_doc.getHash()),
      hypha::Content(OWNER, creator)
    }
  };

  hypha::Document milestone_v_doc(get_self(), creator, std::move(milestone_v_cgs));

  hypha::Edge::write(get_self(), creator, milestone_doc.getHash(), milestone_v_doc.getHash(), graph::VARIABLE);
  hypha::Edge::write(get_self(), creator, quest_hash, milestone_doc.getHash(), graph::HAS_MILESTONE);
  hypha::Edge::write(get_self(), creator, milestone_doc.getHash(), quest_hash, graph::MILESTONE_OF);

}

ACTION quests::delmilestone (checksum256 milestone_hash) {

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);
  hypha::ContentWrapper milestone_cw = milestone_doc.getContentWrapper();

  check_type(milestone_doc, graph::MILESTONE);

  checksum256 quest_hash = milestone_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  hypha::Document quest_doc(get_self(), quest_hash);
  name creator = quest_doc.getCreator();

  require_auth(creator);
  check_quest_status_stage(quest_hash, ""_n, quest_stage_staged, "quests: can not delete milestone");

  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();
  int64_t unfinished_milestones = quest_v_cw.getOrFail(VARIABLE_DETAILS, UNFINISHED_MILESTONES) -> getAs<int64_t>();

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(UNFINISHED_MILESTONES, unfinished_milestones - 1)
  });

  m_documentGraph.eraseDocument(milestone_v_doc.getHash(), true);
  m_documentGraph.eraseDocument(milestone_hash, true);

}

// used by the quest creator
ACTION quests::activate (checksum256 quest_hash) {

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);

  name creator = quest_doc.getCreator();

  require_auth(creator);
  check_type(quest_doc, graph::QUEST);

  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();

  asset amount = quest_cw.getOrFail(FIXED_DETAILS, AMOUNT) -> getAs<asset>();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  check_quest_status_stage(quest_v_cw, ""_n, quest_stage_staged, "quests: can not activate the quest");
  validate_milestones(quest_hash);

  if (fund != creator) {
    hypha::Document account_creator_doc = get_account_info(creator, true);
    hypha::Edge::write(get_self(), get_self(), quest_hash, account_creator_doc.getHash(), graph::VALIDATE);

    update_node(&quest_v_doc, VARIABLE_DETAILS, {
      hypha::Content(STAGE, quest_stage_proposed)
    });

    propose_aux(quest_hash, creator, name("propactivate"), name("notactivate"));
    return;
  }

  hypha::Document account_creator_doc = get_account_info(creator, false);
  hypha::Document account_creator_v_doc = get_variable_node_or_fail(account_creator_doc);
  hypha::ContentWrapper account_creator_v_cw = account_creator_v_doc.getContentWrapper();

  asset staked_amount = account_creator_v_cw.getOrFail(VARIABLE_DETAILS, ACCOUNT_BALANCE) -> getAs<asset>();
  asset locked_balance = account_creator_v_cw.getOrFail(VARIABLE_DETAILS, LOCKED_BALANCE) -> getAs<asset>();

  check(staked_amount >= amount, "quests: not enough balance to activate the quest");

  update_node(&account_creator_v_doc, VARIABLE_DETAILS, {
    hypha::Content(ACCOUNT_BALANCE, staked_amount - amount),
    hypha::Content(LOCKED_BALANCE, locked_balance + amount)
  });

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STAGE, quest_stage_active)
  });

  print("ACTIVATE action (", creator, ") executed successfully\n");

}

// used by the contract inside a proposal
ACTION quests::propactivate (checksum256 quest_hash) {

  require_auth(get_self());

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STAGE, quest_stage_active)
  });

  print("PROP ACTIVATE action executed successfully\n");

}

// used by the contract inside a proposal
ACTION quests::notactivate (checksum256 quest_hash) {

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);

  require_auth(get_self());

  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();
  name stage = quest_v_cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
  if (stage != quest_stage_proposed) { return; }

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STAGE, quest_stage_staged)
  });

  name creator = quest_doc.getCreator();

  print("NOT ACTIVATE action (", creator, ") executed successfully\n");

}

// used by the quest cretor
ACTION quests::delquest (checksum256 quest_hash) {

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);

  name creator = quest_doc.getCreator();

  check_type(quest_doc, graph::QUEST);
  require_auth(creator);

  check_quest_status_stage(quest_doc.getHash(), ""_n, quest_stage_staged, "quests: can not cancel the quest");

  m_documentGraph.eraseDocument(quest_doc.getHash(), true);
  m_documentGraph.eraseDocument(quest_v_doc.getHash(), true);

  print("CANCEL action executed successfully\n");

}

ACTION quests::apply (checksum256 quest_hash, name applicant, string description) {

  require_auth(applicant);
  check_user(applicant);

  check_quest_status_stage(quest_hash, ""_n, quest_stage_active, "quests: user can not apply for this quest");

  hypha::Document quest_doc(get_self(), quest_hash);
  check_type(quest_doc, graph::QUEST);

  hypha::ContentGroups applicant_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::APPLICANT),
      hypha::Content(APPLICANT_ACCOUNT, applicant)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(QUEST_HASH, quest_hash),
      hypha::Content(OWNER, applicant)
    }
  };

  hypha::Document applicant_doc(get_self(), applicant, std::move(applicant_cgs));

  hypha::ContentGroups applicant_v_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
      hypha::Content(DESCRIPTION, description),
      hypha::Content(STATUS, applicant_status_pending)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(APPLICANT_HASH, applicant_doc.getHash()),
      hypha::Content(OWNER, applicant)
    }
  };

  hypha::Document applicant_v_doc(get_self(), applicant, std::move(applicant_v_cgs));

  hypha::Edge::write(get_self(), applicant, quest_hash, applicant_doc.getHash(), graph::HAS_APPLICANT);
  hypha::Edge::write(get_self(), applicant, applicant_doc.getHash(), applicant_v_doc.getHash(), graph::VARIABLE);

  if (is_voted_quest(quest_doc)) {
    propose_aux(applicant_doc.getHash(), quest_doc.getCreator(), name("accptapplcnt"), name("rejctapplcnt"));
  }

}

// used by the contract inside a proposal (voted quest) or the quest creator (private quest)
ACTION quests::accptapplcnt (checksum256 applicant_hash) {

  hypha::Document applicant_doc(get_self(), applicant_hash);
  hypha::Document applicant_v_doc = get_variable_node_or_fail(applicant_doc);
  hypha::ContentWrapper applicant_cw = applicant_doc.getContentWrapper();

  check_type(applicant_doc, graph::APPLICANT);

  checksum256 quest_hash = applicant_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();

  hypha::Document quest_doc(get_self(), quest_hash);
  name creator = quest_doc.getCreator();

  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  check_auth(creator, fund);

  bool exists = edge_exists(quest_doc.getHash(), graph::HAS_ACCPTAPPL);

  if (creator == fund) {
    check_quest_status_stage(quest_hash, ""_n, quest_stage_active, "quests: can not accept applicant");
    check(!exists, "quests: quest already has an accepted applicant");
  } else {
    if (exists) { print("ACCPTAPPLICNT not executed\n"); return; }
    hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
    hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();
    name stage = quest_v_cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
    if (stage != quest_stage_active) { return; }
  }

  update_node(&applicant_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_accepted),
    hypha::Content(ACCEPTED_DATE, int64_t(eosio::current_time_point().sec_since_epoch()))
  });

  hypha::Edge::write(get_self(), creator, quest_hash, applicant_doc.getHash(), graph::HAS_ACCPTAPPL);

  name applicant_account = applicant_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();
  print("ACCPTAPPLCNT action executed successfully (", applicant_account, ")\n");

}

// used by the contract inside a proposal (voted quest) or the quest creator (private quest)
ACTION quests::rejctapplcnt (checksum256 applicant_hash) {

  hypha::Document applicant_doc(get_self(), applicant_hash);
  hypha::Document applicant_v_doc = get_variable_node_or_fail(applicant_doc);
  hypha::ContentWrapper applicant_cw = applicant_doc.getContentWrapper();

  check_type(applicant_doc, graph::APPLICANT);

  checksum256 quest_hash = applicant_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name creator = quest_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();
  check_auth(creator, fund);

  update_node(&applicant_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_rejected)
  });

  print("REJCTAPPLCNT action executed successfully\n");

}

// the applicant selected as the maker accepts the quest
ACTION quests::accptquest (checksum256 quest_hash) {

  check_quest_status_stage(quest_hash, ""_n, quest_stage_active, "quests: applicant can not accept this quest");

  hypha::Document quest_doc(get_self(), quest_hash);
  check_type(quest_doc, graph::QUEST);

  hypha::Document maker_doc = get_doc_from_edge(quest_hash, graph::HAS_ACCPTAPPL);
  hypha::Document maker_v_doc = get_variable_node_or_fail(maker_doc);
  hypha::ContentWrapper maker_cw = maker_doc.getContentWrapper();
  hypha::ContentWrapper maker_v_cw = maker_v_doc.getContentWrapper();

  name maker = maker_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();

  require_auth(maker);

  update_node(&maker_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_confirmed)
  });

  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STARTED_DATE, int64_t(eosio::current_time_point().sec_since_epoch()))
  });

  hypha::Edge maker_edge = hypha::Edge::get(get_self(), quest_hash, maker_doc.getHash(), graph::HAS_ACCPTAPPL);
  maker_edge.erase();

  hypha::Edge::write(get_self(), quest_doc.getCreator(), quest_hash, maker_doc.getHash(), graph::HAS_MAKER);

}

// the maker completes a milestone
ACTION quests::mcomplete (checksum256 milestone_hash, string url_documentation, string description) {

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);

  check_type(milestone_doc, graph::MILESTONE);

  hypha::Document quest_doc = get_quest_node_from_milestone(milestone_doc);
  hypha::Document maker_doc = get_doc_from_edge(quest_doc.getHash(), graph::HAS_MAKER);

  check_quest_status_stage(quest_doc.getHash(), ""_n, quest_stage_active, "quests: user can not complete the milestone");

  hypha::ContentWrapper maker_cw = maker_doc.getContentWrapper();
  hypha::ContentWrapper milestone_v_cw = milestone_v_doc.getContentWrapper();

  name maker = maker_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();
  require_auth(maker);

  name milestone_status = milestone_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
  check(milestone_status == milestone_status_not_completed, "quests: milestone must be not completed");

  update_node(&milestone_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, milestone_status_completed),
    hypha::Content(URL_DOCUMENTATION, url_documentation),
    hypha::Content(DESCRIPTION, description)
  });

  if (is_voted_quest(quest_doc)) {
    propose_aux(milestone_hash, quest_doc.getCreator(), name("propaccptmil"), name("rejctmilstne"));
  }

}

void quests::accept_milestone (hypha::Document & milestone_doc, hypha::Document & milestone_v_doc) {

  hypha::ContentWrapper milestone_cw = milestone_doc.getContentWrapper();
  asset payout_amount = milestone_cw.getOrFail(FIXED_DETAILS, PAYOUT_AMOUNT) -> getAs<asset>();

  update_milestone_status(&milestone_v_doc, milestone_status_finished, milestone_status_completed);

  hypha::Document quest_doc = get_quest_node_from_milestone(milestone_doc);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();

  int64_t unfinished_milestones = quest_v_cw.getOrFail(VARIABLE_DETAILS, UNFINISHED_MILESTONES) -> getAs<int64_t>();
  asset current_payout_amount = quest_v_cw.getOrFail(VARIABLE_DETAILS, CURRENT_PAYOUT_AMOUNT) -> getAs<asset>();

  unfinished_milestones--;

  if (unfinished_milestones == 0) {
    update_node(&quest_v_doc, VARIABLE_DETAILS, {
      hypha::Content(UNFINISHED_MILESTONES, unfinished_milestones),
      hypha::Content(CURRENT_PAYOUT_AMOUNT, current_payout_amount + payout_amount),
      hypha::Content(STATUS, quest_status_finished),
      hypha::Content(STAGE, quest_stage_done)
    });
  } else {
    update_node(&quest_v_doc, VARIABLE_DETAILS, {
      hypha::Content(UNFINISHED_MILESTONES, unfinished_milestones),
      hypha::Content(CURRENT_PAYOUT_AMOUNT, current_payout_amount + payout_amount)
    });
  }

}

// used by the quest creator
ACTION quests::accptmilstne (checksum256 milestone_hash) {

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);

  check_type(milestone_doc, graph::MILESTONE);

  hypha::Document quest_doc = get_quest_node_from_milestone(milestone_doc);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();
  name creator = quest_doc.getCreator();

  check_auth(creator, fund);

  accept_milestone(milestone_doc, milestone_v_doc);

  print("ACCPTMILESTONE executed successfully\n");

}

// used by the contract inside a proposal
ACTION quests::propaccptmil (checksum256 milestone_hash) {

  require_auth(get_self());

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);

  accept_milestone(milestone_doc, milestone_v_doc);

  print("PROP ACCPT MIL executed successfully\n");

}

// pays out an accepted milestone, can be called by anyone
ACTION quests::payoutmilstn (checksum256 milestone_hash) {

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);
  hypha::Document quest_doc = get_quest_node_from_milestone(milestone_doc);
  hypha::Document maker_doc = get_doc_from_edge(quest_doc.getHash(), graph::HAS_MAKER);

  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  hypha::ContentWrapper milestone_cw = milestone_doc.getContentWrapper();
  hypha::ContentWrapper milestone_v_cw = milestone_v_doc.getContentWrapper();

  name status = milestone_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
  check(status == milestone_status_finished, "quests: the milestone is not finished");

  int64_t is_paid_out = milestone_v_cw.getOrFail(VARIABLE_DETAILS, IS_PAID_OUT) -> getAs<int64_t>();
  check(is_paid_out == int64_t(0), "quests: the milestone has been paid out already");

  name maker = maker_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  hypha::Document account_info_doc = get_account_info(fund, false);
  hypha::Document account_info_v_doc = get_variable_node_or_fail(account_info_doc);
  hypha::ContentWrapper account_info_v_cw = account_info_v_doc.getContentWrapper();

  asset locked_balance = account_info_v_cw.getOrFail(VARIABLE_DETAILS, LOCKED_BALANCE) -> getAs<asset>();
  asset payout_amount = milestone_cw.getOrFail(FIXED_DETAILS, PAYOUT_AMOUNT) -> getAs<asset>();

  send_to_escrow(get_self(), maker, payout_amount, "milestone payout");

  update_node(&account_info_v_doc, VARIABLE_DETAILS, {
    hypha::Content(LOCKED_BALANCE, locked_balance - payout_amount)
  });

  update_node(&milestone_v_doc, VARIABLE_DETAILS, {
    hypha::Content(IS_PAID_OUT, int64_t(1))
  });

}

// used by the contract inside a proposal (voted quest) or the quest creator (private quest)
ACTION quests::rejctmilstne (checksum256 milestone_hash) {

  hypha::Document milestone_doc(get_self(), milestone_hash);
  hypha::Document milestone_v_doc = get_variable_node_or_fail(milestone_doc);

  check_type(milestone_doc, graph::MILESTONE);

  hypha::Document quest_doc = get_quest_node_from_milestone(milestone_doc);
  name creator = quest_doc.getCreator();

  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  check_auth(creator, fund);

  update_milestone_status(&milestone_v_doc, milestone_status_not_completed, milestone_status_completed);

}

void quests::propose_aux (const checksum256 & node_hash, const name & quest_owner, const name & passed_action, const name & rejected_action) {

  hypha::Document node_doc(get_self(), node_hash);
  hypha::ContentWrapper node_cw = node_doc.getContentWrapper();

  name node_type = node_cw.getOrFail(FIXED_DETAILS, TYPE) -> getAs<name>();
  name proposal_type = get_proposal_type(node_type);

  check(proposal_type != name("none"), "quests: invalid proposal type");

  asset amount = asset(0, utils::seeds_symbol);

  if (passed_action == "propactivate"_n) {
    amount = node_cw.getOrFail(FIXED_DETAILS, AMOUNT) -> getAs<asset>();
  }

  hypha::ContentGroups proposal_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::PROPOSAL),
      hypha::Content(PROPOSAL_TYPE, proposal_type),
      hypha::Content(PASSED_ACTION, passed_action),
      hypha::Content(REJECTED_ACTION, rejected_action),
      hypha::Content(AMOUNT, amount),
      hypha::Content(QUEST_OWNER, quest_owner)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(DATE_OF_PROPOSAL, int64_t(eosio::current_time_point().sec_since_epoch())),
      hypha::Content(NODE_HASH, node_hash)
    }
  };

  hypha::Document proposal_doc(get_self(), get_self(), std::move(proposal_cgs));

  hypha::ContentGroups proposal_v_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
      hypha::Content(FAVOUR, int64_t(0)),
      hypha::Content(AGAINST, int64_t(0)),
      hypha::Content(STATUS, proposal_status_open),
      hypha::Content(STAGE, proposal_stage_staged),
      hypha::Content(PASSED_CYCLE, 0),
      hypha::Content(TOTAL_VOTES, int64_t(0))
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(PROPOSAL_HASH, proposal_doc.getHash())
    }
  };

  hypha::Document proposal_v_doc(get_self(), get_self(), std::move(proposal_v_cgs));

  hypha::Edge::write(get_self(), get_self(), proposal_doc.getHash(), node_doc.getHash(), graph::PROPOSE);
  hypha::Edge::write(get_self(), get_self(), node_doc.getHash(), proposal_doc.getHash(), graph::PROPOSED_BY);

  hypha::Edge::write(get_self(), get_self(), proposal_doc.getHash(), proposal_v_doc.getHash(), graph::VARIABLE);

  hypha::Document proposals_doc = get_proposals_node();
  hypha::Edge::write(get_self(), get_self(), proposals_doc.getHash(), proposal_doc.getHash(), graph::OPEN);

}

uint64_t quests::get_quorum() {
  return config_get("quest.quorum"_n);
}

uint64_t quests::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}

// determines whether a prop passes or not, executes the corresponding action
ACTION quests::evalprop (checksum256 proposal_hash) {

  hypha::Document proposal_doc(get_self(), proposal_hash);
  hypha::ContentWrapper proposal_cw = proposal_doc.getContentWrapper();

  hypha::Document proposal_v_doc = get_variable_node_or_fail(proposal_doc);
  hypha::ContentWrapper proposal_v_cw = proposal_v_doc.getContentWrapper();

  if(has_auth(get_self())) {
    require_auth(get_self());
  } else {
    name quest_owner = proposal_cw.getOrFail(FIXED_DETAILS, QUEST_OWNER) -> getAs<name>();
    require_auth(quest_owner);
  }

  uint64_t majority = config_get(name("prop.q.mjrty"));
  uint64_t total_eligible_voters = get_size(user_active_size);
  uint64_t quorum = get_quorum();

  name stage = proposal_v_cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
  uint64_t voters_number = uint64_t(proposal_v_cw.getOrFail(VARIABLE_DETAILS, TOTAL_VOTES) -> getAs<int64_t>());

  if (stage == proposal_stage_active) {

    int64_t favour = proposal_v_cw.getOrFail(VARIABLE_DETAILS, FAVOUR) -> getAs<int64_t>();
    int64_t against = proposal_v_cw.getOrFail(VARIABLE_DETAILS, AGAINST) -> getAs<int64_t>();

    bool proposal_passed = utils::is_valid_majority(favour, against, majority); // favour > 0 && double(favour) >= (favour + against) * majority;
    bool valid_quorum = utils::is_valid_quorum(voters_number, quorum, total_eligible_voters);

    print("voters_number:", voters_number, ", quorum:", quorum, ", total_eligible_voters:", total_eligible_voters, "\n");

    hypha::Document proposals_doc = get_proposals_node();

    (hypha::Edge::get(get_self(), proposals_doc.getHash(), graph::OPEN)).erase();

    if (proposal_passed && valid_quorum) {

      update_node(&proposal_v_doc, VARIABLE_DETAILS, {
        hypha::Content(STATUS, proposal_status_passed),
        hypha::Content(STAGE, proposal_stage_done)
      });

      hypha::Edge::write(get_self(), get_self(), proposals_doc.getHash(), proposal_doc.getHash(), graph::PASSED);

      name passed_action = proposal_cw.getOrFail(FIXED_DETAILS, PASSED_ACTION) -> getAs<name>();
      checksum256 node_hash = proposal_cw.getOrFail(IDENTIFIER_DETAILS, NODE_HASH) -> getAs<checksum256>();

      if (passed_action == "propactivate"_n) {
        asset amount = proposal_cw.getOrFail(FIXED_DETAILS, AMOUNT) -> getAs<asset>();
        token::transfer_action action{contracts::token, {bankaccts::campaigns, "active"_n}};
        action.send(bankaccts::campaigns, get_self(), amount, "quest supply");
        print("MAKE THE TRANSFER\n");
      }

      print("EXECUTE ACTION:", passed_action, "\n");

      action(
        permission_level(get_self(), "active"_n),
        get_self(),
        passed_action,
        std::make_tuple(node_hash)
      ).send();

    } else {

      update_node(&proposal_v_doc, VARIABLE_DETAILS, {
        hypha::Content(STATUS, proposal_status_rejected),
        hypha::Content(STAGE, proposal_stage_done)
      });

      hypha::Edge::write(get_self(), get_self(), proposals_doc.getHash(), proposal_doc.getHash(), graph::REJECTED);

      // execute rejection action if any
      name rejected_action = proposal_cw.getOrFail(FIXED_DETAILS, REJECTED_ACTION) -> getAs<name>();
      checksum256 node_hash = proposal_cw.getOrFail(IDENTIFIER_DETAILS, NODE_HASH) -> getAs<checksum256>();

      print("EXECUTE ACTION:", rejected_action, "\n");

      if (rejected_action != ""_n) {
        action(
          permission_level(get_self(), "active"_n),
          get_self(),
          rejected_action,
          std::make_tuple(node_hash)
        ).send();
      }

    }

  } else if (stage == proposal_stage_staged) {
    update_node(&proposal_v_doc, VARIABLE_DETAILS, {
      hypha::Content(STAGE, proposal_stage_active)
    });
  }

}

int64_t quests::active_cutoff_date() {
  uint64_t now = current_time_point().sec_since_epoch();
  uint64_t prop_cycle_sec = config_get(name("propcyclesec"));
  uint64_t inact_cycles = config_get(name("inact.cyc"));
  return now - (inact_cycles * prop_cycle_sec);
}

bool quests::is_active(hypha::ContentWrapper & account_info_v_cw, int64_t & cutoff_date) {
  int64_t last_activity = account_info_v_cw.getOrFail(VARIABLE_DETAILS, LAST_ACTIVITY) -> getAs<int64_t>();
  return last_activity > cutoff_date;
}


void quests::vote_aux (name & voter, const checksum256 & proposal_hash, int64_t & amount, const name & option) {

  hypha::Document proposal_doc(get_self(), proposal_hash);
  hypha::Document proposal_v_doc = get_variable_node_or_fail(proposal_doc);
  hypha::ContentWrapper proposal_cw = proposal_doc.getContentWrapper(); 
  hypha::ContentWrapper proposal_v_cw = proposal_v_doc.getContentWrapper();

  check_type(proposal_doc, graph::PROPOSAL);

  name proposal_stage = proposal_v_cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
  name proposal_type = proposal_cw.getOrFail(FIXED_DETAILS, PROPOSAL_TYPE) -> getAs<name>();
  checksum256 node_hash = proposal_cw.getOrFail(IDENTIFIER_DETAILS, NODE_HASH) -> getAs<checksum256>();
  hypha::Document node_doc(get_self(), node_hash);

  check(proposal_stage == proposal_stage_active, "can not vote for this proposal, it is not active");
  check(!edge_exists(proposal_hash, voter), "quests: only one vote");

  hypha::Document account_info_doc = get_account_info(voter, true);

  if (proposal_type == proposal_type_quest) {
    hypha::Edge::getOrNew(get_self(), get_self(), node_doc.getHash(), account_info_doc.getHash(), graph::VALIDATE);
  } else {
    hypha::ContentWrapper node_cw = node_doc.getContentWrapper();
    checksum256 quest_hash = node_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
    check(hypha::Edge::exists(get_self(), quest_hash, account_info_doc.getHash(), graph::VALIDATE), 
      "quests: user can not vote for this proposal as it is not a validator");
  }

  action(
    permission_level(contracts::proposals, "active"_n),
    contracts::proposals,
    "questvote"_n,
    std::make_tuple(voter, amount, true, contracts::proposals)
  ).send();

  string label;
  int64_t new_value;
  int64_t favour_amount = 0;
  int64_t against_amount = 0;

  if (option == trust) {
    label = FAVOUR;
    new_value = amount + proposal_v_cw.getOrFail(VARIABLE_DETAILS, FAVOUR) -> getAs<int64_t>();
    favour_amount = amount;
  } else if (option == distrust) {
    label = AGAINST;
    new_value = amount + proposal_v_cw.getOrFail(VARIABLE_DETAILS, AGAINST) -> getAs<int64_t>();
    against_amount = amount;
  }

  int64_t total_votes = proposal_v_cw.getOrFail(VARIABLE_DETAILS, TOTAL_VOTES) -> getAs<int64_t>();

  update_node(&proposal_v_doc, VARIABLE_DETAILS, {
    hypha::Content(label, new_value),
    hypha::Content(TOTAL_VOTES, total_votes + 1)
  });

  hypha::ContentGroups vote_cgs {
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
      hypha::Content(TYPE, graph::VOTE),
      hypha::Content(FAVOUR, favour_amount),
      hypha::Content(AGAINST, against_amount)
    },
    hypha::ContentGroup {
      hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
      hypha::Content(PROPOSAL_HASH, proposal_hash),
      hypha::Content(OWNER, voter)
    }
  };

  hypha::Document vote_doc(get_self(), voter, std::move(vote_cgs));

  hypha::Edge::write(get_self(), voter, proposal_hash, vote_doc.getHash(), voter);
  hypha::Edge::write(get_self(), voter, proposal_hash, vote_doc.getHash(), graph::VOTED_BY);

}

ACTION quests::favour (name voter, checksum256 proposal_hash, int64_t amount) {
  require_auth(voter);
  vote_aux(voter, proposal_hash, amount, trust);
}

ACTION quests::against (name voter, checksum256 proposal_hash, int64_t amount) {
  require_auth(voter);
  vote_aux(voter, proposal_hash, amount, distrust);
}

// can expire if the quest is taking too long
ACTION quests::expirequest (checksum256 quest_hash) {

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();

  check_type(quest_doc, graph::QUEST);

  name creator = quest_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  asset amount = quest_cw.getOrFail(FIXED_DETAILS, AMOUNT) -> getAs<asset>();
  asset current_payout = quest_v_cw.getOrFail(VARIABLE_DETAILS, CURRENT_PAYOUT_AMOUNT) -> getAs<asset>();

  int64_t duration = quest_cw.getOrFail(FIXED_DETAILS, DURATION) -> getAs<int64_t>();
  int64_t started_date = quest_v_cw.getOrFail(VARIABLE_DETAILS, STARTED_DATE) -> getAs<int64_t>();
  int64_t now = int64_t(eosio::current_time_point().sec_since_epoch());

  check(started_date + duration <= now, "quests: can not expire the quest, it is too soon");

  bool create_proposal = check_auth_create_proposal(creator, fund);

  if (create_proposal) {
    propose_aux(quest_hash, creator, name("expirequest"), ""_n);
    return;
  }

  hypha::Document fund_doc = get_account_info(fund, false);
  hypha::Document fund_v_doc = get_variable_node_or_fail(fund_doc);
  hypha::ContentWrapper fund_v_cw = fund_v_doc.getContentWrapper();

  asset locked_balance = fund_v_cw.getOrFail(VARIABLE_DETAILS, LOCKED_BALANCE) -> getAs<asset>();
  asset refund_amount = amount - current_payout;

  if (has_auth(get_self())) {
    token::transfer_action action{contracts::token, {get_self(), "active"_n}};
    action.send(get_self(), bankaccts::campaigns, refund_amount, "quest refund");

    hypha::Document balances_doc = get_account_infos_node();
    hypha::Document balances_v_doc = get_variable_node_or_fail(balances_doc);
    sub_balance(balances_v_doc, refund_amount);

    update_node(&fund_v_doc, VARIABLE_DETAILS, {
      hypha::Content(LOCKED_BALANCE, locked_balance - refund_amount)
    });
  } else {
    asset account_balance = fund_v_cw.getOrFail(VARIABLE_DETAILS, ACCOUNT_BALANCE) -> getAs<asset>();
    update_node(&fund_v_doc, VARIABLE_DETAILS, {
      hypha::Content(ACCOUNT_BALANCE, account_balance + refund_amount),
      hypha::Content(LOCKED_BALANCE, locked_balance - refund_amount)
    });
  }

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, quest_status_expired),
    hypha::Content(STAGE, quest_stage_done)
  });

}

// can expire applicant if he/she is taking too long to accept the quest
ACTION quests::expireappl (checksum256 maker_hash) {

  hypha::Document maker_doc(get_self(), maker_hash);
  hypha::ContentWrapper maker_cw = maker_doc.getContentWrapper();

  check_type(maker_doc, graph::APPLICANT);

  hypha::Document maker_v_doc = get_variable_node_or_fail(maker_doc);
  hypha::ContentWrapper maker_v_cw = maker_v_doc.getContentWrapper();

  name status = maker_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
  check(status == applicant_status_accepted, "quests: can not expire applicant, the applicant is not in accepted status");

  int64_t accepted_date = maker_v_cw.getOrFail(VARIABLE_DETAILS, ACCEPTED_DATE) -> getAs<int64_t>();
  int64_t waiting_seconds = config_get(name("qst.exp.appl"));
  int64_t now = int64_t(eosio::current_time_point().sec_since_epoch());
  int64_t cutoff = now - waiting_seconds;

  check(accepted_date <= cutoff, "quests: can not expire applicant, it is too soon");

  checksum256 quest_hash = maker_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name creator = quest_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  bool create_proposal = check_auth_create_proposal(creator, fund);

  if (create_proposal) {
    propose_aux(maker_hash, creator, name("expireappl"), ""_n);
    return;
  }

  hypha::Edge maker_edge = hypha::Edge::get(get_self(), quest_doc.getHash(), maker_doc.getHash(), graph::HAS_ACCPTAPPL);
  maker_edge.erase();

  update_node(&maker_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_expired)
  });

}

// cancel the maker
ACTION quests::cancelappl (checksum256 maker_hash) {

  hypha::Document maker_doc(get_self(), maker_hash);
  hypha::ContentWrapper maker_cw = maker_doc.getContentWrapper();

  check_type(maker_doc, graph::APPLICANT);

  hypha::Document maker_v_doc = get_variable_node_or_fail(maker_doc);
  hypha::ContentWrapper maker_v_cw = maker_v_doc.getContentWrapper();

  name status = maker_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
  check(status == applicant_status_confirmed, "quests: can not cancel applicant, the applicant is not in confirmed status");

  checksum256 quest_hash = maker_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name creator = quest_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  bool create_proposal = check_auth_create_proposal(creator, fund);

  if (create_proposal) {
    propose_aux(maker_hash, creator, name("cancelappl"), ""_n);
    return;
  }

  hypha::Edge maker_edge = hypha::Edge::get(get_self(), quest_doc.getHash(), maker_doc.getHash(), graph::HAS_MAKER);
  maker_edge.erase();

  update_node(&maker_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_cancel)
  });

}

// cancel the application if not getting any response (used by the applicant)
ACTION quests::retractappl (checksum256 applicant_hash) {

  hypha::Document applicant_doc(get_self(), applicant_hash);
  hypha::ContentWrapper applicant_cw = applicant_doc.getContentWrapper();

  check_type(applicant_doc, graph::APPLICANT);

  name applicant_account = applicant_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();
  require_auth(applicant_account);

  hypha::Document applicant_v_doc = get_variable_node_or_fail(applicant_doc);
  hypha::ContentWrapper applicant_v_cw = applicant_v_doc.getContentWrapper();

  name status = applicant_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
  check(status == applicant_status_pending, "quests: can not retract application");

  checksum256 quest_hash = applicant_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  
  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name creator = quest_doc.getCreator();
  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();

  if (creator != fund) {
    hypha::Edge edge = hypha::Edge::get(get_self(), applicant_hash, graph::PROPOSED_BY);
    hypha::Document proposal_doc(get_self(), edge.getToNode());
    hypha::Document proposal_v_doc = get_variable_node_or_fail(proposal_doc);
    hypha::ContentWrapper proposal_v_cw = proposal_v_doc.getContentWrapper();

    name proposal_stage = proposal_v_cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
    check(proposal_stage == proposal_stage_staged, "quests: can not retract application");

    m_documentGraph.eraseDocument(proposal_doc.getHash(), true);
    m_documentGraph.eraseDocument(proposal_v_doc.getHash(), true);
  }

  m_documentGraph.eraseDocument(applicant_doc.getHash(), true);
  m_documentGraph.eraseDocument(applicant_v_doc.getHash(), true);

}

// abandone a quest (if the applicant is the maker)
ACTION quests::quitapplcnt (checksum256 applicant_hash) {

  hypha::Document applicant_doc(get_self(), applicant_hash);
  hypha::ContentWrapper applicant_cw = applicant_doc.getContentWrapper();

  check_type(applicant_doc, graph::APPLICANT);

  name applicant_account = applicant_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();
  checksum256 quest_hash = applicant_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();

  require_auth(applicant_account);
  std::pair<bool, hypha::Edge> p = hypha::Edge::getIfExists(get_self(), quest_hash, graph::HAS_MAKER);

  check(p.first, "quests: quest does not have a maker");
  check(p.second.getToNode() == applicant_hash, "quests: applicant is not a maker");

  hypha::Document applicant_v_doc = get_variable_node_or_fail(applicant_doc);

  update_node(&applicant_v_doc, VARIABLE_DETAILS, {
    hypha::Content(STATUS, applicant_status_quitted)
  });

  p.second.erase();

}

ACTION quests::rateapplcnt (checksum256 maker_hash, name opinion) {

  hypha::Document maker_doc(get_self(), maker_hash);
  hypha::ContentWrapper maker_cw = maker_doc.getContentWrapper();

  check_type(maker_doc, graph::APPLICANT);
  check(opinion == like || opinion == dislike, "quests: invalid opinion");

  checksum256 quest_hash = maker_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  check(hypha::Edge::exists(get_self(), quest_hash, maker_hash, graph::HAS_MAKER), "quests: this applicant is not a maker");

  check_quest_status_stage(quest_hash, quest_status_finished, quest_stage_done, "quests: can not rate applicant");

  hypha::Document quest_doc(get_self(), quest_hash);
  name creator = quest_doc.getCreator();

  require_auth(creator);

  hypha::Document maker_v_doc = get_variable_node_or_fail(maker_doc);
  hypha::ContentWrapper maker_v_cw = maker_v_doc.getContentWrapper();

  auto [idx, item] = maker_v_cw.get(VARIABLE_DETAILS, OWNER_OPINION);
  check(idx == -1, "the applicant has already been rated");

  if (opinion == like) {
    name applicant_account = maker_cw.getOrFail(FIXED_DETAILS, APPLICANT_ACCOUNT) -> getAs<name>();
    uint64_t rep = config_get(name("qst.rep.appl"));
    action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(applicant_account, rep)
    ).send();
  }

  update_node(&maker_v_doc, VARIABLE_DETAILS, {
    hypha::Content(OWNER_OPINION, opinion)
  });

}

ACTION quests::ratequest (checksum256 quest_hash, name opinion) {

  hypha::Document quest_doc(get_self(), quest_hash);
  hypha::Edge edge = hypha::Edge::get(get_self(), quest_hash, graph::HAS_MAKER);

  hypha::Document maker_doc(get_self(), edge.getToNode());
  name applicant_account = maker_doc.getCreator();

  require_auth(applicant_account);
  check_quest_status_stage(quest_hash, quest_status_finished, quest_stage_done, "quests: can not rate quest");

  hypha::Document quest_v_doc = get_variable_node_or_fail(quest_doc);
  hypha::ContentWrapper quest_v_cw = quest_v_doc.getContentWrapper();

  auto [idx, item] = quest_v_cw.get(VARIABLE_DETAILS, APPLICANT_OPINION);
  check(idx == -1, "the quest has already been rated");

  if (opinion == like) {
    name creator = quest_doc.getCreator();
    uint64_t rep = config_get(name("qst.rep.qst"));
    action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addrep"_n,
      std::make_tuple(creator, rep)
    ).send();
  }

  update_node(&quest_v_doc, VARIABLE_DETAILS, {
    hypha::Content(APPLICANT_OPINION, opinion)
  });

}

void quests::check_type (hypha::Document & node_doc, const name & type) {
  hypha::ContentWrapper node_cw = node_doc.getContentWrapper();
  name node_doc_type = node_cw.getOrFail(FIXED_DETAILS, TYPE) -> getAs<name>();
  check(node_doc_type == type, "quests: the node must be of type " + type.to_string());
}

bool quests::check_auth_create_proposal(name & creator, name & fund) {
  bool create_proposal = false;
  bool has_auth_creator = has_auth(creator);

  if (fund == creator) {
    require_auth(creator);
  } else {
    if (has_auth_creator) {
      create_proposal = true;
      require_auth(creator);
    } else {
      require_auth(get_self());
    }
  }

  return create_proposal;
}

void quests::update_node (hypha::Document * node_doc, const string & content_group_label, const std::vector<hypha::Content> & new_contents) {

  checksum256 old_node_hash = node_doc -> getHash();

  hypha::ContentWrapper node_cw = node_doc -> getContentWrapper();
  hypha::ContentGroup * node_cg = node_cw.getGroupOrFail(content_group_label);

  for (int i = 0; i < new_contents.size(); i++) {
    hypha::ContentWrapper::insertOrReplace(*node_cg, new_contents[i]);
  }

  m_documentGraph.updateDocument(get_self(), old_node_hash, node_doc -> getContentGroups());

}

name quests::get_proposal_type (name & node_type) {
  
  name proposal_type;

  if (node_type == graph::QUEST) {
    proposal_type = proposal_type_quest;
  } else if (node_type == graph::MILESTONE) {
    proposal_type = proposal_type_milestone;
  } else if (node_type == graph::APPLICANT) {
    proposal_type = proposal_type_applicant;
  } else {
    proposal_type = name("none");
  }

  return proposal_type;

}

hypha::Document quests::get_root_node () {

  document_table d_t(get_self(), get_self().value);
  auto root_itr = d_t.begin();

  check(root_itr != d_t.end(), "There is no root node");

  hypha::Document root_doc(get_self(), root_itr -> getHash());
  return root_doc;

}

hypha::Document quests::get_doc_from_edge (const checksum256 & node_hash, const name & edge_name) {
  std::vector<hypha::Edge> edges = m_documentGraph.getEdgesFromOrFail(node_hash, edge_name);
  hypha::Document node_to(get_self(), edges[0].getToNode());
  return node_to;
}

hypha::Document quests::get_account_infos_node () {
  hypha::Document root_doc = get_root_node();
  return get_doc_from_edge(root_doc.getHash(), graph::OWNS_ACCOUNT_INFOS);
}

hypha::Document quests::get_proposals_node () {
  hypha::Document root_doc = get_root_node();
  return get_doc_from_edge(root_doc.getHash(), graph::OWNS_PROPOSALS);
}

hypha::Document quests::get_variable_node_or_fail (hypha::Document & fixed_node) {
  return get_doc_from_edge(fixed_node.getHash(), graph::VARIABLE);
}

void quests::check_auth (name & creator, name & fund) {
  if (fund == creator) {
    require_auth(creator);
  } else {
    require_auth(get_self());
  }
}

void quests::check_quest_status_stage (const checksum256 & quest_hash, const name & status, const name & stage, const string & error_msg) {

  hypha::Document quest_doc(get_self(), quest_hash);
  std::vector<hypha::Edge> edges = m_documentGraph.getEdgesFromOrFail(quest_hash, graph::VARIABLE);

  hypha::Document quest_v_doc(get_self(), edges[0].getToNode());
  hypha::ContentWrapper cw = quest_v_doc.getContentWrapper();

  check_quest_status_stage(cw, status, stage, error_msg);

}

void quests::check_quest_status_stage (hypha::ContentWrapper & cw, const name & status, const name & stage, const string & error_msg) {

  if (status != ""_n) {
    name current_status = cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
    check(current_status == status, error_msg + ": the quest status is not " + status.to_string());
  }

  if (stage != ""_n) {
    name current_stage = cw.getOrFail(VARIABLE_DETAILS, STAGE) -> getAs<name>();
    check(current_stage == stage, error_msg + ": the quest stage is not " + stage.to_string());
  }

}

void quests::validate_milestones (const checksum256 & quest_hash) {

  std::vector<hypha::Edge> edges = m_documentGraph.getEdgesFromOrFail(quest_hash, graph::HAS_MILESTONE);
  int64_t total = 0;

  for (int i = 0; i < edges.size(); i++) {

    hypha::Document milestone_doc(get_self(), edges[i].getToNode());
    hypha::ContentWrapper cw = milestone_doc.getContentWrapper();

    hypha::Content * milestone_content = cw.getOrFail(FIXED_DETAILS, PAYOUT_PERCENTAGE);
    total += std::get<int64_t>(milestone_content -> value);

  }

  check(total == 10000, "The total payout of the milestones must be 100.00%");

}

void quests::update_balance (hypha::Document & balance_doc, asset & quantity, const bool & substract) {

    checksum256 oldHash = balance_doc.getHash();
    hypha::ContentWrapper old_cw = balance_doc.getContentWrapper();

    asset old_balance_asset = old_cw.getOrFail(VARIABLE_DETAILS, ACCOUNT_BALANCE) -> getAs<asset>();

    hypha::Content new_balance;

    if (substract) {
      check(old_balance_asset >= quantity, "quests: not enough balance");
      new_balance = hypha::Content(ACCOUNT_BALANCE, old_balance_asset - quantity);
    } else {
      new_balance = hypha::Content(ACCOUNT_BALANCE, old_balance_asset + quantity);
    }

    hypha::ContentGroup * cg = old_cw.getGroupOrFail(VARIABLE_DETAILS);
    hypha::ContentWrapper::insertOrReplace(*cg, new_balance);

    m_documentGraph.updateDocument(get_self(), oldHash, balance_doc.getContentGroups());

}

void quests::add_balance (hypha::Document & balance_doc, asset & quantity) {
  utils::check_asset(quantity);
  update_balance(balance_doc, quantity, false);
}

void quests::sub_balance (hypha::Document & balance_doc, asset & quantity) {
  utils::check_asset(quantity);
  update_balance(balance_doc, quantity, true);
}

hypha::Document quests::get_account_info (name & account, const bool & create_if_not_exists) {

  hypha::Document account_infos_doc = get_account_infos_node();
  std::vector<hypha::Edge> edges = m_documentGraph.getEdgesFrom(account_infos_doc.getHash(), account);

  if (create_if_not_exists) {
    if (edges.size() > 0) {
      hypha::Document account_info_doc(get_self(), edges[0].getToNode());
      return account_info_doc;
    } else {
      hypha::ContentGroups account_info_cgs {
        hypha::ContentGroup {
          hypha::Content(hypha::CONTENT_GROUP_LABEL, FIXED_DETAILS),
          hypha::Content(TYPE, graph::ACCOUNT_INFO)
        },
        hypha::ContentGroup {
          hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
          hypha::Content(OWNER, account)
        }
      };

      hypha::ContentGroups account_info_v_cgs {
        hypha::ContentGroup {
          hypha::Content(hypha::CONTENT_GROUP_LABEL, VARIABLE_DETAILS),
          hypha::Content(ACCOUNT_BALANCE, asset(0, utils::seeds_symbol)),
          hypha::Content(LOCKED_BALANCE, asset(0, utils::seeds_symbol))
        },
        hypha::ContentGroup {
          hypha::Content(hypha::CONTENT_GROUP_LABEL, IDENTIFIER_DETAILS),
          hypha::Content(OWNER, account)
        }
      };

      hypha::Document account_info_doc(get_self(), get_self(), std::move(account_info_cgs));
      hypha::Document account_info_v_doc(get_self(), get_self(), std::move(account_info_v_cgs));

      hypha::Edge::write(get_self(), get_self(), account_infos_doc.getHash(), account_info_doc.getHash(), account);
      hypha::Edge::write(get_self(), get_self(), account_info_doc.getHash(), account_info_v_doc.getHash(), graph::VARIABLE);

      return account_info_doc;
    }
  } else {
    check(edges.size() > 0, "quests: account has no info entry");
    hypha::Document account_info_doc(get_self(), edges[0].getToNode());
    return account_info_doc;
  }

}

void quests::check_user(name & account) {
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "quests: user not found");
}

hypha::Document quests::get_quest_node_from_milestone (hypha::Document & milestone_doc) {

  hypha::ContentWrapper milestone_cw = milestone_doc.getContentWrapper();

  checksum256 quest_hash = milestone_cw.getOrFail(IDENTIFIER_DETAILS, QUEST_HASH) -> getAs<checksum256>();
  hypha::Document quest_doc(get_self(), quest_hash);

  return quest_doc;

}

void quests::update_milestone_status (hypha::Document * milestone_v_doc, const name & new_status, const name & check_status) {
  
  checksum256 old_hash = milestone_v_doc -> getHash();
  
  hypha::ContentWrapper milestone_v_cw = milestone_v_doc -> getContentWrapper();

  if (check_status != ""_n) {
    name current_status = milestone_v_cw.getOrFail(VARIABLE_DETAILS, STATUS) -> getAs<name>();
    check(current_status == check_status, "quests: milestone status must be " + check_status.to_string());
  }

  hypha::ContentGroup * cg = milestone_v_cw.getGroupOrFail(VARIABLE_DETAILS);
  hypha::ContentWrapper::insertOrReplace(*cg, hypha::Content(STATUS, new_status));
  
  m_documentGraph.updateDocument(get_self(), old_hash, milestone_v_doc -> getContentGroups());

}

void quests::send_to_escrow (const name & fromfund, const name & recipient, asset & quantity, const string & memo) {

  action(
    permission_level{fromfund, "active"_n},
    contracts::token, "transfer"_n,
    std::make_tuple(fromfund, contracts::escrow, quantity, memo))
  .send();

  action(
      permission_level{fromfund, "active"_n},
      contracts::escrow, "lock"_n,
      std::make_tuple("time"_n, 
                      fromfund,
                      recipient,
                      quantity,
                      ""_n,
                      ""_n,
                      time_point(current_time_point().time_since_epoch()),  // long time from now
                      memo))
  .send();

  hypha::Document balances_doc = get_account_infos_node();
  hypha::Document balances_v_doc = get_variable_node_or_fail(balances_doc);
  sub_balance(balances_v_doc, quantity);

  print("SEND TO ESCROW\n");

}

bool quests::edge_exists (const checksum256 & from_node_hash, const name & edge_name) {
  std::vector<hypha::Edge> edges = m_documentGraph.getEdgesFrom(from_node_hash, edge_name);
  return edges.size() > 0;
}

bool quests::is_voted_quest (hypha::Document & quest_doc) {
  hypha::ContentWrapper quest_cw = quest_doc.getContentWrapper();

  name fund = quest_cw.getOrFail(FIXED_DETAILS, FUND) -> getAs<name>();
  name creator = quest_doc.getCreator();

  return fund != creator;
}
