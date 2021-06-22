#pragma once

#include "referendum_base.hpp"


class ReferendumSettings : public Referendum {

  public:

    using Referendum::Referendum;

    void create(std::map<std::string, VariantValue> & args) override;

    void update(std::map<std::string, VariantValue> & args) override;

    void cancel(std::map<std::string, VariantValue> & args) override;

    void evaluate(std::map<std::string, VariantValue> & args) override;

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

    name get_next_status(const std::vector<uint64_t> & cycles_per_status, const uint64_t & age);

    SettingInfo * get_setting_info (const name & setting_name);

};

