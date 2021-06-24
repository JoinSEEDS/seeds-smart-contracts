#pragma once

#include <contracts.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>
#include <tables/user_table.hpp>
#include <tables/referendums_table.hpp>
#include <tables/vote_table.hpp>
#include <tables/size_table.hpp>


using namespace eosio;
using std::string;
using std::make_tuple;
using std::distance;


CONTRACT referendums : public contract {
  public:
      using contract::contract;
      referendums(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          config(contracts::settings, contracts::settings.value)
          {}

      ACTION reset();

      ACTION initcycle(const uint64_t & cycle_id);

      ACTION stake(const name & from, const name & to, const asset & quantity, const string & memo);

      ACTION create(std::map<std::string, VariantValue> & args);

      ACTION update(std::map<std::string, VariantValue> & args);

      ACTION cancel(std::map<std::string, VariantValue> & args);

      ACTION onperiod();

      ACTION evaluate(const uint64_t & referendum_id);

      ACTION favour(const name & voter, const uint64_t & referendum_id, const uint64_t & amount);

      ACTION against(const name & voter, const uint64_t & referendum_id, const uint64_t & amount);

      ACTION neutral(const name & voter, const uint64_t & referendum_id);


      uint64_t get_required_quorum(const name & setting, const bool & is_float);

      uint64_t get_required_unity(const name & setting, const bool & is_float);


      template <typename... T>
      void send_deferred_transaction(const permission_level & permission, const name & contract, const name & action, const std::tuple<T...> & data);

      template <typename... T>
      void send_inline_action(const permission_level & permission, const name & contract, const name & action, const std::tuple<T...> & data);


      DEFINE_REFERENDUM_TABLE
      DEFINE_REFERENDUM_TABLE_MULTI_INDEX

      DEFINE_CONFIG_TABLE
      DEFINE_CONFIG_TABLE_MULTI_INDEX
      DEFINE_CONFIG_GET

      DEFINE_CONFIG_FLOAT_TABLE
      DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

      DEFINE_VOTE_TABLE
      DEFINE_VOTE_TABLE_MULTI_INDEX

      DEFINE_SIZE_TABLE
      DEFINE_SIZE_TABLE_MULTI_INDEX


      TABLE referendum_auxiliary_table {
        uint64_t referendum_id;
        std::map<string, VariantValue> special_attributes;

        EOSLIB_SERIALIZE(referendum_auxiliary_table, (referendum_id)(special_attributes))

        uint64_t primary_key () const { return referendum_id; }
      };
      typedef eosio::multi_index<"refaux"_n, referendum_auxiliary_table> referendum_auxiliary_tables;

      TABLE deferred_id_table {
        uint64_t id;
      };
      typedef singleton<"deferredids"_n, deferred_id_table> deferred_id_tables;
      typedef eosio::multi_index<"deferredids"_n, deferred_id_table> dump_for_deferred_id;

      TABLE cycle_table {
        uint64_t propcycle; 
        uint64_t t_onperiod; // last time onperiod ran
      };
      typedef singleton<"cycle"_n, cycle_table> cycle_tables;
      typedef eosio::multi_index<"cycle"_n, cycle_table> dump_for_cycle;

      config_tables config;


  private:

    static constexpr name high_impact = "high"_n;
    static constexpr name medium_impact = "med"_n;
    static constexpr name low_impact = "low"_n;

    void check_citizen(const name & account);
    void vote_aux(const name & voter, const uint64_t & referendum_id, const uint64_t & amount, const name & option, const bool & is_new);
    bool revert_vote(const name & voter, const uint64_t & referendum_id);
    void check_attributes(const std::map<std::string, VariantValue> & args);

};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<referendums>(name(receiver), name(code), &referendums::stake);
  } else if (code == receiver) {
      switch (action) {
        EOSIO_DISPATCH_HELPER(referendums, 
          (reset)(initcycle)
          (create)(update)(cancel)(onperiod)(evaluate)
          (favour)(against)(neutral)
        )
      }
  }
}
