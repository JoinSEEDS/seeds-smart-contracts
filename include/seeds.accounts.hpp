#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <contracts.hpp>
#include <tables.hpp>
#include <tables/rep_table.hpp>
#include <tables/size_table.hpp>
#include <tables/cbs_table.hpp>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/config_float_table.hpp>
#include <utils.hpp>

using namespace eosio;
using std::string;

CONTRACT accounts : public contract {
  public:
      using contract::contract;
      accounts(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          users(receiver, receiver.value),
          refs(receiver, receiver.value),
          cbs(receiver, receiver.value),
          vouches(receiver, receiver.value),
          vouchtotals(receiver, receiver.value),
          reqvouch(receiver, receiver.value),
          rep(receiver, receiver.value),
          sizes(receiver, receiver.value),
          balances(contracts::harvest, contracts::harvest.value),
          config(contracts::settings, contracts::settings.value),
          configfloat(contracts::settings, contracts::settings.value),
          accts(contracts::token, contracts::token.value),
          actives(contracts::proposals, contracts::proposals.value),
          totals(contracts::history, contracts::history.value),
          residents(contracts::history, contracts::history.value),
          citizens(contracts::history, contracts::history.value),
          history_sizes(contracts::history, contracts::history.value)
          {}

      ACTION reset();

      ACTION adduser(name account, string nickname, name type);

      ACTION makeresident(name user);
      ACTION canresident(name user);

      ACTION makecitizen(name user);
      ACTION cancitizen(name user);

      ACTION update(name user, name type, string nickname, string image, string story, string roles, string skills, string interests);

      ACTION addref(name referrer, name invited);

      ACTION invitevouch(name referrer, name invited);

      ACTION addrep(name user, uint64_t amount);

      ACTION subrep(name user, uint64_t amount);

      ACTION requestvouch(name account, name sponsor);

      ACTION vouch(name sponsor, name account);
      ACTION unvouch(name sponsor, name account);
      ACTION pnishvouched(name sponsor, uint64_t start_account);

      ACTION rankreps();
      ACTION rankrep(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

      ACTION rankcbss();
      ACTION rankcbs(uint64_t start_val, uint64_t chunk, uint64_t chunksize);

      ACTION changesize(name id, int64_t delta);

      ACTION flag(name from, name to);
      ACTION removeflag(name from, name to);
      ACTION punish(name account, uint64_t points);
      ACTION pnshvouchers(name account, uint64_t points, uint64_t start);
      ACTION evaldemote(name to, uint64_t start_val, uint64_t chunk, uint64_t chunksize);

      ACTION testresident(name user);
      ACTION testcitizen(name user);
      ACTION testvisitor(name user);
      ACTION testremove(name user);
      ACTION testsetrep(name user, uint64_t amount);
      ACTION testsetrs(name user, uint64_t amount);
      ACTION testsetcbs(name user, uint64_t amount);
      ACTION testreward();

      ACTION testmvouch(name sponsor, name account, uint64_t reps);
      ACTION migratevouch(uint64_t start_user, uint64_t start_sponsor, uint64_t batch_size);

  private:
      symbol seeds_symbol = symbol("SEEDS", 4);
      symbol network_symbol = symbol("TLOS", 4);

      const name individual = "individual"_n;
      const name organization = "organisation"_n;

      const name not_found = ""_n;

      const name reputation_reward_resident = "refrep1.ind"_n;
      const name reputation_reward_citizen = "refrep2.ind"_n;

      const name individual_seeds_reward_resident = "refrwd1.ind"_n;
      const name individual_seeds_reward_citizen = "refrwd2.ind"_n;
      const name min_individual_seeds_reward_resident = "minrwd1.ind"_n;
      const name min_individual_seeds_reward_citizen = "minrwd2.ind"_n;
      const name dec_individual_seeds_reward_resident = "decrwd1.ind"_n;
      const name dec_individual_seeds_reward_citizen = "decrwd2.ind"_n;

      const name org_seeds_reward_resident = "refrwd1.org"_n;
      const name org_seeds_reward_citizen = "refrwd2.org"_n;
      const name min_org_seeds_reward_resident = "minrwd1.org"_n;
      const name min_org_seeds_reward_citizen = "minrwd2.org"_n;
      const name dec_org_seeds_reward_resident = "decrwd1.org"_n;
      const name dec_org_seeds_reward_citizen = "decrwd2.org"_n;

      const name ambassador_seeds_reward_resident = "refrwd1.amb"_n;
      const name ambassador_seeds_reward_citizen = "refrwd2.amb"_n;
      const name min_ambassador_seeds_reward_resident = "minrwd1.amb"_n;
      const name min_ambassador_seeds_reward_citizen = "minrwd2.amb"_n;
      const name dec_ambassador_seeds_reward_resident = "decrwd1.amb"_n;
      const name dec_ambassador_seeds_reward_citizen = "decrwd2.amb"_n;

      const name max_vouch_points = "maxvouch"_n;

      const name resident_vouch_points = "res.vouch"_n;
      const name citizen_vouch_points = "cit.vouch"_n;

      const name flag_total_scope = "flag.total"_n;
      const name flag_remove_scope = "flag.remove"_n;

      void buyaccount(name account, string owner_key, string active_key);
      void check_user(name account);
      void rewards(name account, name new_status);
      void vouchreward(name account);
      void refreward(name account, name new_status);
      void send_reward(name beneficiary, asset quantity);
      void updatestatus(name account, name status);
      void _vouch(name sponsor, name account);
      void history_add_resident(name account);
      void history_add_citizen(name account);
      name find_referrer(name account);
      void send_addrep(name user, uint64_t amount);
      void send_subrep(name user, uint64_t amount);
      void send_to_escrow(name fromfund, name recipient, asset quantity, string memo);
      uint64_t countrefs(name user, int check_num_residents);
      uint64_t rep_score(name user);
      void add_rep_item(name account, uint64_t reputation);
      uint64_t config_get(name key);
      double config_float_get(name key);
      void size_change(name id, int delta);
      void size_set(name id, uint64_t newsize);
      uint64_t get_size(name id);
      bool check_can_make_resident(name user);
      bool check_can_make_citizen(name user);
      uint32_t num_transactions(name account, uint32_t limit);
      void add_active (name user);
      void add_cbs(name account, int points);
      void send_punish(name account, uint64_t points);
      void send_eval_demote(name to);
      void send_punish_vouchers(name account, uint64_t points);
      void calc_vouch_rep(name account);

      void migrate_calc_vouch_rep(name account); // migration - remove

      DEFINE_USER_TABLE

      DEFINE_USER_TABLE_MULTI_INDEX

      DEFINE_REP_TABLE

      DEFINE_REP_TABLE_MULTI_INDEX

      DEFINE_SIZE_TABLE

      DEFINE_SIZE_TABLE_MULTI_INDEX

      DEFINE_CBS_TABLE

      DEFINE_CBS_TABLE_MULTI_INDEX

      TABLE ref_table {
        name referrer;
        name invited;

        uint64_t primary_key() const { return invited.value; }
        uint64_t by_referrer()const { return referrer.value; }
      };

      TABLE vouch_table {
        name account;
        name sponsor;
        uint64_t reps;

        uint64_t primary_key() const { return sponsor.value; }
        uint64_t by_account()const { return account.value; }

      };

      TABLE vouches_table {
        uint64_t id;
        name account;
        name sponsor;
        uint64_t vouch_points;

        uint64_t primary_key() const { return id; }
        uint64_t by_account()const { return account.value; }
        uint64_t by_sponsor()const { return sponsor.value; }
        uint128_t by_sponsor_account()const { return (uint128_t(sponsor.value) << 64) + account.value; }
        uint128_t by_account_sponsor()const { return (uint128_t(account.value) << 64) + sponsor.value; }
      };

      TABLE vouches_totals_table {
        name account;
        uint64_t total_vouch_points;
        uint64_t total_rep_points;

        uint64_t primary_key() const { return account.value; }
      };

      TABLE req_vouch_table {
        uint64_t id;
        name account;
        name sponsor;

        uint64_t primary_key() const { return id; }
        uint64_t by_account()const { return account.value; }
        uint64_t by_sponsor()const { return sponsor.value; }
      };

      TABLE flag_points_table { // scoped by receiving user
        name account;
        uint64_t flag_points;

        uint64_t primary_key() const { return account.value; }
      };

      typedef eosio::multi_index<"flagpts"_n, flag_points_table> flag_points_tables;

    DEFINE_CONFIG_TABLE

    DEFINE_CONFIG_TABLE_MULTI_INDEX

    DEFINE_CONFIG_FLOAT_TABLE

    DEFINE_CONFIG_FLOAT_TABLE_MULTI_INDEX

      // Borrowed from histry.seeds contract
      TABLE citizen_table {
        uint64_t id;
        name account;
        uint64_t timestamp;
        
        uint64_t primary_key()const { return id; }
        uint64_t by_account()const { return account.value; }
      };
      TABLE resident_table {
        uint64_t id;
        name account;
        uint64_t timestamp;
        
        uint64_t primary_key()const { return id; }
        uint64_t by_account()const { return account.value; }
      };
      typedef eosio::multi_index<"citizens"_n, citizen_table,
        indexed_by<"byaccount"_n,
        const_mem_fun<citizen_table, uint64_t, &citizen_table::by_account>>
      > citizen_tables;
      typedef eosio::multi_index<"residents"_n, resident_table, 
        indexed_by<"byaccount"_n,
        const_mem_fun<resident_table, uint64_t, &resident_table::by_account>>
      > resident_tables;
      // END from histry.seeds contract

    typedef eosio::multi_index<"refs"_n, ref_table,
      indexed_by<"byreferrer"_n,const_mem_fun<ref_table, uint64_t, &ref_table::by_referrer>>
    > ref_tables;
    
    typedef eosio::multi_index<"vouch"_n, vouch_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<vouch_table, uint64_t, &vouch_table::by_account>>
    > vouch_tables;

    typedef eosio::multi_index<"vouches"_n, vouches_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<vouches_table, uint64_t, &vouches_table::by_account>>,
      indexed_by<"bysponsor"_n,
      const_mem_fun<vouches_table, uint64_t, &vouches_table::by_sponsor>>,
      indexed_by<"byspnsoracct"_n,
      const_mem_fun<vouches_table, uint128_t, &vouches_table::by_sponsor_account>>,
      indexed_by<"byacctspnsor"_n,
      const_mem_fun<vouches_table, uint128_t, &vouches_table::by_account_sponsor>>
    > vouches_tables;

    typedef eosio::multi_index<"vouchtotals"_n, vouches_totals_table> vouches_totals_tables;

    typedef eosio::multi_index<"reqvouch"_n, req_vouch_table,
      indexed_by<"byaccount"_n,
      const_mem_fun<req_vouch_table, uint64_t, &req_vouch_table::by_account>>,
      indexed_by<"bysponsor"_n,
      const_mem_fun<req_vouch_table, uint64_t, &req_vouch_table::by_sponsor>>
    > req_vouch_tables;

    typedef eosio::multi_index<"balances"_n, tables::balance_table,
        indexed_by<"byplanted"_n,
        const_mem_fun<tables::balance_table, uint64_t, &tables::balance_table::by_planted>>
    > balance_tables;
    balance_tables balances;

    struct [[eosio::table]] account {
      asset    balance;

      uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index< "accounts"_n, account > token_accts;
    token_accts accts; 

    cbs_tables cbs;
    ref_tables refs;
    vouches_tables vouches;
    vouches_totals_tables vouchtotals;
    req_vouch_tables reqvouch;
    user_tables users;
    rep_tables rep;
    size_tables sizes;

    size_tables history_sizes;
    resident_tables residents;
    citizen_tables citizens;

    config_tables config;
    config_float_tables configfloat;

    // From history contract
    TABLE totals_table {
      name account;
      uint64_t total_volume;
      uint64_t total_number_of_transactions;
      uint64_t total_incoming_from_rep_orgs;
      uint64_t total_outgoing_to_rep_orgs;

      uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"totals"_n, totals_table> totals_tables;
    totals_tables totals;

    // From proposals contract
    TABLE active_table {
      name account;
      uint64_t timestamp;
      bool active;

      uint64_t primary_key()const { return account.value; }
    };
    typedef eosio::multi_index<"actives"_n, active_table> active_tables;
    active_tables actives;

};

EOSIO_DISPATCH(accounts, (reset)(adduser)(canresident)(makeresident)(cancitizen)(makecitizen)(update)(addref)(invitevouch)(addrep)(changesize)
(subrep)(testsetrep)(testsetrs)(testcitizen)(testresident)(testvisitor)(testremove)(testsetcbs)
(testreward)(requestvouch)(vouch)(unvouch)(pnishvouched)
(rankreps)(rankrep)(rankcbss)(rankcbs)
(flag)(removeflag)(punish)(pnshvouchers)(evaldemote)
(testmvouch)(migratevouch)
);
