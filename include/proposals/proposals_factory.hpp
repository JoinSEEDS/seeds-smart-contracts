#pragma once

#include "referendum_settings.hpp"


class ProposalsFactory {

  public:

    static Proposal * Factory(referendums & _contract, const name & type) {
      switch (type.value)
      {
      case ProposalsCommon::type_ref_setting.value:
        return new ReferendumSettings(_contract);
      
      default:
        break;
      }

      check(false, "Unknown proposal type " + type.to_string());
      return nullptr;
    }

};

