#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>


using namespace eosio;
using std::string;


CONTRACT voice : public contract {
  
  public:

    using contract::contract;
    voice(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        sizes(receiver, receiver.value)
        {}

    ACTION reset();

    ACTION setvoice(const name & user, const uint64_t & amount, const uint64_t & scope);

    ACTION addvoice(const name & user, const uint64_t & amount, const uint64_t & scope);

    ACTION changetrust(const name & user, const bool & trust);

    ACTION vote(const name & voter, const uint64_t & amount, const name & scope);


  private:

    const name alliance_scope = "alliance"_n;
    const name campaign_scope = contracts::proposals;
    const name milestone_scope = "milestone"_n;
    const name referendums_scope = contracts::referendums;

    void set_voice(const name & user, const uint64_t & amount, const name & scope);
    double voice_change(const name & user, const uint64_t & amount, const bool & reduce, const name & scope);
    void erase_voice(const name & user);

    std::vector<name> scopes = {
      alliance_scope,
      campaign_scope,
      milestone_scope,
      referendums_scope
    };

    TABLE voice_table {
      name account;
      uint64_t balance;

      uint64_t primary_key()const { return account.value; }
    };

    typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX
    DEFINE_SIZE_CHANGE

    size_tables sizes;

};


EOSIO_DISPATCH(voice, (reset)(setvoice)(addvoice)(changetrust)(vote))

