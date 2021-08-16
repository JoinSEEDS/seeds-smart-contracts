#pragma once

#include "proposals_base.hpp"


class ProposalCampaignInvite : public Proposal {

  public:
  
    using Proposal::Proposal;

    name get_scope() override;
    name get_fund_type() override;

    void callback(std::map<std::string, VariantValue> & args) override;

    void create_impl(std::map<std::string, VariantValue> & args) override;
    void update_impl(std::map<std::string, VariantValue> & args) override;

    void status_open_impl(std::map<std::string, VariantValue> & args) override;
    void status_eval_impl(std::map<std::string, VariantValue> & args) override;
    void status_rejected_impl(std::map<std::string, VariantValue> & args) override;

};