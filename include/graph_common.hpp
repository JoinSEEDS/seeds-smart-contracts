#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

namespace graph {

  // document type
  static constexpr name ROOT = name("root");
  static constexpr name QUEST = name("quest");
  static constexpr name MILESTONE = name("milestone");
  static constexpr name APPLICANT = name("applicant");
  static constexpr name PROPOSALS = name("proposals");
  static constexpr name PROPOSAL = name("proposal");
  static constexpr name ACCOUNT_INFOS = name("accntinfos");
  static constexpr name ACCOUNT_INFO = name("accntinfo");
  static constexpr name VOTE = name("vote");

  // graph edges
  static constexpr name VARIABLE = name("variable");
  static constexpr name HAS_QUEST = name("hasquest");
  static constexpr name MILESTONE_OF = name("milestoneof");
  static constexpr name HAS_MILESTONE = name("hasmilestone");
  static constexpr name HAS_APPLICANT = name("hasapplicant");
  static constexpr name HAS_ACCPTAPPL = name("hasaccptappl");
  static constexpr name HAS_MAKER = name("hasmaker");
  static constexpr name PROPOSED_BY = name("proposedby");
  static constexpr name PROPOSE = name("propose");
  static constexpr name OPEN = name("open");
  static constexpr name PASSED = name("passed");
  static constexpr name REJECTED = name("rejected");
  static constexpr name OWNS_ACCOUNT_INFOS = name("ownaccntinfs");
  static constexpr name OWNS_PROPOSALS = name("ownproposals");
  static constexpr name OWNED_BY = name("ownedby");
  static constexpr name VOTE_FOR = name("votefor");
  static constexpr name VOTED_BY = name("votedby");
  static constexpr name VALIDATE = name("validate");
  static constexpr name CREATE = name("create");

  #define NOT_FOUND -1

  #define QUEST_HASH "quest_hash"
  #define MILESTONE_HASH "milestone_hash"
  #define APPLICANT_HASH "applicant_hash"
  #define FIXED_DETAILS "fixed_details"
  #define VARIABLE_DETAILS "variable_details"
  #define IDENTIFIER_DETAILS "identifier_details"
  #define OWNER "owner"
  #define TYPE "type"
  #define TITLE "title"
  #define DESCRIPTION "description"
  #define AMOUNT "amount"
  #define FUND "fund"
  #define STATUS "status"
  #define STAGE "stage"
  #define PAYOUT_PERCENTAGE "payout_percentage"
  #define URL_DOCUMENTATION "url_documentation"
  #define ACCOUNT_BALANCE "account_balance"
  #define LOCKED_BALANCE "locked_balance"
  #define CURRENT_PAYOUT_AMOUNT "current_payout_amount"
  #define DUE_DATE "due_date"
  #define APPLICANT_ACCOUNT "applicant_account"
  #define PROPOSAL_TYPE "proposal_type"
  #define NODE_HASH "node_hash"
  #define FAVOUR "favour"
  #define AGAINST "against"
  #define PASSED_CYCLE "passed_cycle"
  #define PROPOSAL_HASH "proposal_hash"
  #define OPEN_PROPOSALS "open_proposals"
  #define ACCOUNT_VOICE "account_voice"
  #define PASSED_ACTION "passed_action"
  #define REJECTED_ACTION "rejected_action"
  #define TOTAL_VOTES "total_votes"
  #define LAST_ACTIVITY "last_activity"
  #define NUMBER_OF_MILESTONES "num_milestones"
  #define UNFINISHED_MILESTONES "unfinished_milestones"
  #define DATE_OF_PROPOSAL "date_of_proposal"
  #define STARTED_DATE "started_date"
  #define ACCEPTED_DATE "accepted_date"
  #define DURATION "duration"
  #define OWNER_OPINION "owner_opinion"
  #define APPLICANT_OPINION "applicant_opinion"
  #define PAYOUT_AMOUNT "payout_amount"
  #define IS_PAID_OUT "is_paid_out"
  #define QUEST_OWNER "quest_owner"

}
