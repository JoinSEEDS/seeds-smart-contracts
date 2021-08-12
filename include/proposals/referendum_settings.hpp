#pragma once

#include "proposals_base.hpp"


class ReferendumSettings : public Proposal {

  public:

    using Proposal::Proposal;

    void create_impl(std::map<std::string, VariantValue> & args) override;
    void update_impl(std::map<std::string, VariantValue> & args) override;

    void evaluate(std::map<std::string, VariantValue> & args) override;

    name get_scope() override;
    name get_fund_type() override;

    void check_can_vote(const name & status, const name & stage) override;
    uint64_t min_stake(const asset & quantity, const name & fund) override;

    typedef struct sttg_info 
    {
      uint64_t previous_value_uint;
      double previous_value_double;
      bool is_float;
    } 
    SettingInfo;

  private:

    static constexpr name high_impact = name("high");
    static constexpr name medium_impact = name("med");
    static constexpr name low_impact = name("low");

    uint64_t get_required_unity(const name & setting, const bool & is_float);
    uint64_t get_required_quorum(const name & setting, const bool & is_float);
    name get_next_status(const string & cycles_per_status_string, const uint64_t & age);

    SettingInfo * get_setting_info (const name & setting_name);

    template <typename T>
    void change_setting(const name & setting_name, const T & setting_value, const bool & is_float);

};

