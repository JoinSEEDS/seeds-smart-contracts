#pragma once

#include "proposals_base.hpp"


class ProposalAlliance : public Proposal {

  public:

    using Proposal::Proposal;

    void callback(std::map<std::string, VariantValue> & args) override;


    name get_scope() override;
    name get_fund_type() override;


    void create_impl(std::map<std::string, VariantValue> & args) override;
    void update_impl(std::map<std::string, VariantValue> & args) override;

    void status_open_impl(std::map<std::string, VariantValue> & args) override;
    void status_eval_impl(std::map<std::string, VariantValue> & args) override;
    void status_rejected_impl(std::map<std::string, VariantValue> & args) override;

};

