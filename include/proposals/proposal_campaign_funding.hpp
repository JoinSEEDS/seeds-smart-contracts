#pragma once

#include "proposals_base.hpp"


class ProposalCampaignFunding : public Proposal {

  public:

    using Proposal::Proposal;

    void create_impl(std::map<std::string, VariantValue> & args) override;
    void update_impl(std::map<std::string, VariantValue> & args) override;
    void status_open_impl (std::map<std::string, VariantValue> & args) override;
    void status_eval_impl(std::map<std::string, VariantValue> & args) override;

    name get_scope() override;
    name get_fund_type() override;


  private:

    std::vector<uint64_t> * values_to_vector(const string & values);
    void check_percentages(std::vector<uint64_t> & pay_percentages);
    asset get_payout_amount(std::vector<uint64_t> & pay_percentages, const uint64_t & age, const asset & total_amount, const asset & current_payout);

};
