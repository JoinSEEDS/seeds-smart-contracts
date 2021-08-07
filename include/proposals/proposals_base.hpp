#pragma once

#include <seeds.dao.hpp>

namespace ProposalsCommon {
  constexpr name type_ref_setting = name("r.setting");
  constexpr name type_ref_code = name("r.code");
  constexpr name type_prop_alliance = name("p.alliance");
  
  constexpr name status_open = name("open");
  constexpr name status_voting = name("voting");
  constexpr name status_test = name("test");
  constexpr name status_evaluate = name("evaluate");
  constexpr name status_passed = name("passed");
  constexpr name status_rejected = name("rejected");

  constexpr name stage_staged = name("staged");
  constexpr name stage_active = name("active");
  constexpr name stage_done = name("done");

  constexpr name trust = name("trust");
  constexpr name distrust = name("distrust");
  constexpr name neutral = name("neutral");

  constexpr name vote_scope = name("votes");

  constexpr name fund_type_none = name("none");
}

class Proposal {

  public:

    Proposal(dao & _contract) : m_contract(_contract) {};
    virtual ~Proposal(){};

    virtual void create(std::map<std::string, VariantValue> & args);
    virtual void update(std::map<std::string, VariantValue> & args);
    virtual void cancel(std::map<std::string, VariantValue> & args);
    virtual void evaluate(std::map<std::string, VariantValue> & args);
    virtual void callback(std::map<std::string, VariantValue> & args);


    virtual name get_scope() = 0;
    virtual name get_fund_type() = 0;


    virtual void check_can_vote(const name & status, const name & stage);
    virtual bool check_prop_majority(std::map<std::string, VariantValue> & args);


    virtual void create_impl(std::map<std::string, VariantValue> & args);
    virtual void update_impl(std::map<std::string, VariantValue> & args);
    virtual void cancel_impl(std::map<std::string, VariantValue> & args);
    virtual void status_open_impl(std::map<std::string, VariantValue> & args);
    virtual void status_eval_impl(std::map<std::string, VariantValue> & args);
    virtual void status_rejected_impl(std::map<std::string, VariantValue> & args);


    dao & m_contract;

};
