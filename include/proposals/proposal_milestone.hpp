#pragma once

#include "proposals_base.hpp"


class ProposalMilestone: public Proposal {

  public:

    using Proposal::Proposal;

    name get_scope() override;
    name get_fund_type() override;

    void create_impl(std::map<std::string, VariantValue> & args) override;
    void status_open_impl(std::map<std::string, VariantValue> & args) override;

};