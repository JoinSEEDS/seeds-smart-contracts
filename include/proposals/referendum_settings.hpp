#pragma once

#include "referendum_base.hpp"

class ReferendumSettings : public Referendum {

  public:
    using Referendum::Referendum;

    void create(std::map<std::string, VariantValue> & args) override;

    void update(std::map<std::string, VariantValue> & args) override;

    void cancel(std::map<std::string, VariantValue> & args) override;

    void evaluate(std::map<std::string, VariantValue> & args) override;

};

