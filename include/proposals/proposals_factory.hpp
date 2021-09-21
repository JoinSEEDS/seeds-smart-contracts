#pragma once

#include "referendum_settings.hpp"
#include "proposal_alliance.hpp"
#include "proposal_campaign_invite.hpp"
#include "proposal_milestone.hpp"
#include "proposal_campaign_funding.hpp"


class ProposalsFactory {

  public:

    static Proposal * Factory(dao & _contract, const name & type) {
      switch (type.value)
      {
      case ProposalsCommon::type_ref_setting.value:
        return new ReferendumSettings(_contract);

      case ProposalsCommon::type_prop_alliance.value:
        return new ProposalAlliance(_contract);

      case ProposalsCommon::type_prop_campaign_invite.value:
        return new ProposalCampaignInvite(_contract);

      case ProposalsCommon::type_prop_milestone.value:
        return new ProposalMilestone(_contract);

      case ProposalsCommon::type_prop_campaign_funding.value:
        return new ProposalCampaignFunding(_contract);
      
      default:
        break;
      }

      check(false, "Unknown proposal type " + type.to_string());
      return nullptr;
    }

};

