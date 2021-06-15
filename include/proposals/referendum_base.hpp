#pragma once

#include <seeds.referendums.hpp>
#include "tables/referendums_table.hpp"

namespace ReferendumsCommon {
  constexpr name type_settings = name("r.setting");
  constexpr name type_code = name("r.code");
  
  constexpr name status_open = name("open");
  constexpr name status_test = name("test");
  constexpr name status_evaluate = name("evaluate");
  constexpr name status_passed = name("passed");
  constexpr name status_rejected = name("rejected");

  constexpr name stage_staged = name("staged");
  constexpr name stage_active = name("active");
  constexpr name stage_done = name("done");
}

class Referendum {

  public:

    Referendum(referendums & _contract) : m_contract(_contract) {};
    virtual ~Referendum(){};

    virtual void create(std::map<std::string, VariantValue> & args) = 0;

    virtual void update(std::map<std::string, VariantValue> & args) = 0;

    virtual void cancel(std::map<std::string, VariantValue> & args) = 0;

    virtual void evaluate(std::map<std::string, VariantValue> & args) = 0;

    referendums & m_contract;

};
