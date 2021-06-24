#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>
#include <tables/cspoints_table.hpp>


using namespace eosio;
using std::string;


CONTRACT voice : public contract {
  
  public:

    using contract::contract;
    voice(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        config(receiver, receiver.value),
        sizes(receiver, receiver.value)
        {}

    ACTION reset();

    ACTION setvoice(const name & user, const uint64_t & amount, const uint64_t & scope);

    ACTION addvoice(const name & user, const uint64_t & amount, const uint64_t & scope);

    ACTION subvoice(const name & user, const uint64_t & amount, const uint64_t & scope);

    ACTION changetrust(const name & user, const bool & trust);

    ACTION vote(const name & voter, const uint64_t & amount, const name & scope);

    ACTION decayvoices();

    ACTION decayvoice(const uint64_t & start, const uint64_t & chunksize);


  private:

    const name alliance_scope = "alliance"_n;
    const name campaign_scope = contracts::proposals;
    const name milestone_scope = "milestone"_n;
    const name referendums_scope = contracts::referendums;

    const name cycle_vote_power_size = "votepow.sz"_n; 

    std::vector<name> scopes = {
      alliance_scope,
      campaign_scope,
      milestone_scope,
      referendums_scope
    };

    void set_voice(const name & user, const uint64_t & amount, const name & scope);
    double voice_change(const name & user, const uint64_t & amount, const bool & reduce, const name & scope);
    void erase_voice(const name & user);
    void recover_voice(const name & account);
    uint64_t calculate_decay(const uint64_t & voice_amount);

    TABLE voice_table {
      name account;
      uint64_t balance;

      uint64_t primary_key()const { return account.value; }
    };

    TABLE cycle_table {
      uint64_t propcycle; 
      uint64_t t_onperiod; // last time onperiod ran
      uint64_t t_voicedecay; // last time voice was decayed
    };

    typedef eosio::multi_index<"voice"_n, voice_table> voice_tables;
    typedef singleton<"cycle"_n, cycle_table> cycle_tables;
    typedef eosio::multi_index<"cycle"_n, cycle_table> dump_for_cycle;

    DEFINE_CONFIG_TABLE
    DEFINE_CONFIG_TABLE_MULTI_INDEX
    DEFINE_CONFIG_GET

    DEFINE_SIZE_TABLE
    DEFINE_SIZE_TABLE_MULTI_INDEX
    DEFINE_SIZE_CHANGE

    config_tables config;
    size_tables sizes;

};


EOSIO_DISPATCH(voice, 
  (reset)(setvoice)(addvoice)(subvoice)(changetrust)(vote)
  (decayvoices)(decayvoice)
)

