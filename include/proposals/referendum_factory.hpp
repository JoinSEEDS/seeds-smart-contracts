#pragma once

#include "referendum_settings.hpp"


class ReferendumFactory {

  public:

    static Referendum * Factory(referendums & _contract, const name & type) {
      switch (type.value)
      {
      case ReferendumsCommon::type_settings.value:
        return new ReferendumSettings(_contract);
      
      default:
        break;
      }

      check(false, "Unknown referendum type " + type.to_string());
      return nullptr;
    }

};

