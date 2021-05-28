#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <contracts.hpp>
#include <utils.hpp>
#include <tables.hpp>
#include <tables/config_table.hpp>
#include <tables/rep_table.hpp>
#include <tables/organization_table.hpp>
#include <cmath> 

using namespace eosio;
using std::string;

CONTRACT organization : public contract {

    public:
        using contract::contract;
        organization(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              organizations(receiver, receiver.value),
              sponsors(receiver, receiver.value),
              apps(receiver, receiver.value),
              dausscores(receiver, receiver.value),
              regenscores(receiver, receiver.value),
              cbsorgs(receiver, receiver.value),
              sizes(receiver, receiver.value),
              avgvotes(receiver, receiver.value),
              refs(contracts::accounts, contracts::accounts.value),
              users(contracts::accounts, contracts::accounts.value),
              balances(contracts::harvest, contracts::harvest.value),
              config(contracts::settings, contracts::settings.value),
              totals(contracts::history, contracts::history.value),
              planted(contracts::harvest, contracts::harvest.value)
              {}
        
        
        // ACTION addorg(name org, name owner);

        ACTION reset();

        ACTION addmember(name organization, name owner, name account, name role); // (manager / owner permission)
        
        ACTION removemember(name organization, name owner, name account); // (manager owner permission)
        
        ACTION changerole(name organization, name owner, name account, name new_role); // (manager / owner permission)
        
        ACTION changeowner(name organization, name owner, name account); // (owner permission)

        ACTION addregen(name organization, name account, uint64_t amount); // adds a fixed number of regen points, possibly modified by the account reputation. account - account adding the regen (account auth)
        
        ACTION subregen(name organization, name account, uint64_t amount); // same as add, just negative (account auth)

        ACTION create(name sponsor, name orgaccount, string orgfullname, string publicKey);

        ACTION destroy(name orgname, name sponsor);

        ACTION refund(name beneficiary, asset quantity);

        ACTION registerapp(name owner, name organization, name appname, string applongname);

        ACTION banapp(name appname);

        ACTION appuse(name appname, name account);

        ACTION calcmappuses();

        ACTION calcmappuse(uint64_t start, uint64_t chunksize, uint64_t threshold, uint64_t cutoff);

        ACTION rankappuses();

        ACTION rankappuse(uint128_t start, uint64_t chunk, uint64_t chunksize);

        ACTION rankregens();

        ACTION rankregen(uint64_t start, uint64_t chunk, uint64_t chunksize);

        ACTION makethrivble(name organization);

        ACTION makeregen(name organization);

        ACTION makesustnble(name organization);

        ACTION makereptable(name organization);

        void deposit(name from, name to, asset quantity, std::string memo);

        ACTION scoretrxs();

        ACTION scoreorgs(name next);

        ACTION testregensc(name organization, uint64_t score);
        ACTION teststatus(name organization, uint64_t status);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);

        const name min_planted = "org.minplant"_n;
        const name regen_score_size = "rs.sz"_n;
        const name cb_score_size = "cbs.sz"_n;
        const name tx_score_size = "txs.sz"_n;
        const name regen_avg = "org.rgnavg"_n;
        const name app_size = "apps.sz"_n;
        const name app_use_size = "appuse.sz"_n;

        const uint64_t status_regular = 0;
        const uint64_t status_reputable = 1;
        const uint64_t status_sustainable = 2;
        const uint64_t status_regenerative = 3;
        const uint64_t status_thrivable = 4;

        std::vector<name> status_names = {
            "regular"_n,
            "reputable"_n,
            "sustainable"_n,
            "regenerative"_n,
            "thrivable"_n
        };

        DEFINE_ORGANIZATION_TABLE

        DEFINE_ORGANIZATION_TABLE_MULTI_INDEX

        TABLE members_table {
            name account;
            name role;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE sponsors_table { // is it posible to have a negative balance?
            name account;
            asset balance;

            uint64_t primary_key() const { return account.value; } 
        };

        TABLE vote_table {
            name account;
            uint64_t timestamp;
            int64_t regen_points;

            uint64_t primary_key() const { return account.value; }
            uint64_t by_regen_points() const {
                uint64_t regen_id = 1;
                regen_id <<= 63;
                return regen_id + regen_points + ((regen_points < 0) ? -1 : 0);
            }
        };

        TABLE avg_vote_table {
            name org_name;
            int64_t total_sum;
            uint64_t num_votes;
            int64_t average;

            uint64_t primary_key() const { return org_name.value; }
        };

        TABLE regen_score_table {
            name org_name;
            int64_t regen_avg;
            uint64_t rank;

            uint64_t primary_key() const { return org_name.value; }
            uint64_t by_regen_avg() const {
                uint64_t regen_id = 1;
                regen_id <<= 63;
                return regen_id + regen_avg + ((regen_avg < 0) ? -1 : 0);
            }
            uint64_t by_rank() const { return rank; }
        };

        TABLE cbs_organization_table {
            name org_name;
            uint32_t community_building_score;
            uint64_t rank;

            uint64_t primary_key() const { return org_name.value; }
            uint64_t by_cbs() const { return (uint64_t(community_building_score) << 32) +  ( (org_name.value <<32) >> 32) ; }
            uint64_t by_rank() const { return rank; }
        };

        TABLE org_tx_table {
            uint64_t id;
            name other;
            bool in;
            asset quantity;
            uint64_t timestamp;

            uint64_t primary_key() const { return id; }
            uint64_t by_timestamp() const { return timestamp; }
            uint64_t by_quantity() const { return quantity.amount; }
            uint128_t by_other() const { return (uint128_t(other.value) << 64) + id; }
        };

        typedef eosio::multi_index<"orgtx"_n, org_tx_table,
            indexed_by<"bytimestamp"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_timestamp>>,
            indexed_by<"byquantity"_n,const_mem_fun<org_tx_table, uint64_t, &org_tx_table::by_quantity>>,
            indexed_by<"byother"_n,const_mem_fun<org_tx_table, uint128_t, &org_tx_table::by_other>>
        > org_tx_tables;

        TABLE ref_table { // from accounts contract
            name referrer;
            name invited;

            uint64_t primary_key() const { return invited.value; }
            uint64_t by_referrer()const { return referrer.value; }
        };

        typedef eosio::multi_index<"refs"_n, ref_table,
            indexed_by<"byreferrer"_n,const_mem_fun<ref_table, uint64_t, &ref_table::by_referrer>>
        > ref_tables;

        DEFINE_CONFIG_TABLE
        
        DEFINE_CONFIG_TABLE_MULTI_INDEX

        DEFINE_SIZE_TABLE

        DEFINE_SIZE_TABLE_MULTI_INDEX

        DEFINE_REP_TABLE

        DEFINE_REP_TABLE_MULTI_INDEX


        TABLE totals_table {
            name account;
            uint64_t total_volume;
            uint64_t total_number_of_transactions;
            uint64_t total_incoming_from_rep_orgs;
            uint64_t total_outgoing_to_rep_orgs;

            uint64_t primary_key() const { return account.value; }
        };

        typedef eosio::multi_index<"totals"_n, totals_table> totals_tables;


        TABLE app_table {
            name app_name;
            name org_name;
            string app_long_name;
            bool is_banned;
            uint64_t number_of_uses;

            uint64_t primary_key() const { return app_name.value; }
            uint64_t by_org() const { return org_name.value; }
        };

        TABLE daus_table { // scoped by appname
            uint64_t id;
            name account;
            uint64_t day; // beginning of day
            uint64_t number_app_uses;
            int64_t points; // number of points the user gives to the app during this day
            
            uint64_t primary_key() const { return id; }
            uint64_t by_account() const { return account.value; }
            uint64_t by_day() const { return day; }
            uint128_t by_day_account() const { return (uint128_t(day) << 64) + account.value; }
            uint128_t by_account_day() const { return (uint128_t(account.value) << 64) + day; }
        };

        TABLE daus_totals_table { // scoped by appname
            uint64_t day;
            int64_t daily_points;
            uint64_t daily_users;

            uint64_t primary_key() const { return day; }
        };

        TABLE daus_score {
            name app_name;
            int64_t total_points;
            uint64_t total_uses;
            uint64_t rank;

            uint64_t primary_key() const { return app_name.value; }
            uint64_t by_total_points() const {
                uint64_t points_id = 1;
                points_id <<= 63;
                return points_id + total_points;
            }
            uint64_t by_total_uses() const { return total_uses; }
            uint64_t by_rank() const { return rank; }
            uint128_t by_total_points_app() const {
                uint64_t points_id = 1;
                points_id <<= 63;
                return (uint128_t(points_id + total_points) << 64) + app_name.value;
            }
        };

        // TABLE dau_history_table {
        //     uint64_t dau_history_id;
        //     name account;
        //     uint64_t date;
        //     uint64_t number_app_uses;

        //     uint64_t primary_key() const { return dau_history_id; }
        //     uint64_t by_account() const { return account.value; }
        //     uint64_t by_date() const { return date; }
        // };

        TABLE planted_table { // from harvest
            name account;
            asset planted;
            uint64_t rank;  

            uint64_t primary_key()const { return account.value; }
            uint128_t by_planted() const { return (uint128_t(planted.amount) << 64) + account.value; } 
            uint64_t by_rank() const { return rank; } 
        };

        typedef eosio::multi_index<"balances"_n, tables::balance_table,
            indexed_by<"byplanted"_n,
            const_mem_fun<tables::balance_table, uint64_t, &tables::balance_table::by_planted>>
        > balance_tables;
    

        typedef eosio::multi_index <"members"_n, members_table> members_tables;

        typedef eosio::multi_index <"sponsors"_n, sponsors_table> sponsors_tables;

        typedef eosio::multi_index <"votes"_n, vote_table,
            indexed_by<"byregen"_n,
            const_mem_fun<vote_table, uint64_t, &vote_table::by_regen_points>>
        > vote_tables;

        typedef eosio::multi_index <"avgvotes"_n, avg_vote_table> avg_vote_tables;

        typedef eosio::multi_index<"users"_n, tables::user_table,
            indexed_by<"byreputation"_n,
            const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
        > user_tables;

        typedef eosio::multi_index<"apps"_n, app_table,
            indexed_by<"byorg"_n,
            const_mem_fun<app_table, uint64_t, &app_table::by_org>>
        > app_tables;

        typedef eosio::multi_index<"daus"_n, daus_table,
            indexed_by<"byaccount"_n,
            const_mem_fun<daus_table, uint64_t, &daus_table::by_account>>,
            indexed_by<"byday"_n,
            const_mem_fun<daus_table, uint64_t, &daus_table::by_account>>,
            indexed_by<"bydayacct"_n,
            const_mem_fun<daus_table, uint128_t, &daus_table::by_day_account>>,
            indexed_by<"byacctday"_n,
            const_mem_fun<daus_table, uint128_t, &daus_table::by_account_day>>
        > daus_tables;

        typedef eosio::multi_index<"daustotals"_n, daus_totals_table> daus_totals_tables;

        typedef eosio::multi_index<"dausscores"_n, daus_score,
            indexed_by<"bytpoints"_n,
            const_mem_fun<daus_score, uint64_t, &daus_score::by_total_points>>,
            indexed_by<"bytuses"_n,
            const_mem_fun<daus_score, uint64_t, &daus_score::by_total_uses>>,
            indexed_by<"byrank"_n,
            const_mem_fun<daus_score, uint64_t, &daus_score::by_rank>>,
            indexed_by<"bytpointsapp"_n,
            const_mem_fun<daus_score, uint128_t, &daus_score::by_total_points_app>>
        > daus_scores;

        // typedef eosio::multi_index<"dauhistory"_n, dau_history_table,
        //     indexed_by<"byaccount"_n,
        //     const_mem_fun<dau_history_table, uint64_t, &dau_history_table::by_account>>,
        //     indexed_by<"bydate"_n,
        //     const_mem_fun<dau_history_table, uint64_t, &dau_history_table::by_date>>
        // > dau_history_tables;

        typedef eosio::multi_index<"regenscores"_n, regen_score_table,
            indexed_by<"byregenavg"_n,
            const_mem_fun<regen_score_table, uint64_t, &regen_score_table::by_regen_avg>>,
            indexed_by<"byrank"_n,
            const_mem_fun<regen_score_table, uint64_t, &regen_score_table::by_rank>>
        > regen_score_tables;

        typedef eosio::multi_index<"cbsorgs"_n, cbs_organization_table,
            indexed_by<"bycbs"_n,
            const_mem_fun<cbs_organization_table, uint64_t, &cbs_organization_table::by_cbs>>,
            indexed_by<"byrank"_n,
            const_mem_fun<cbs_organization_table, uint64_t, &cbs_organization_table::by_rank>>
        > cbs_organization_tables;

        typedef eosio::multi_index<"planted"_n, planted_table,
            indexed_by<"byplanted"_n,const_mem_fun<planted_table, uint128_t, &planted_table::by_planted>>,
            indexed_by<"byrank"_n,const_mem_fun<planted_table, uint64_t, &planted_table::by_rank>>
        > planted_tables;

        organization_tables organizations;
        sponsors_tables sponsors;
        user_tables users;
        config_tables config;
        app_tables apps;
        daus_scores dausscores;
        regen_score_tables regenscores;
        cbs_organization_tables cbsorgs;
        size_tables sizes;
        balance_tables balances;
        ref_tables refs;
        avg_vote_tables avgvotes;
        totals_tables totals;
        planted_tables planted;

        void create_account(name sponsor, name orgaccount, string fullname, string publicKey);
        void check_owner(name organization, name owner);
        void init_balance(name account);
        int64_t getregenp(name account);
        void check_user(name account);
        void revert_previous_vote(name organization, name account);
        void vote(name organization, name account, int64_t regen);
        void check_asset(asset quantity);
        uint64_t get_beginning_of_day_in_seconds();
        uint64_t get_size(name id);
        void increase_size_by_one(name id);
        void decrease_size_by_one(name id);
        uint32_t calc_transaction_points(name organization);
        void update_status(name organization, uint64_t status);
        uint64_t config_get(name key);
        void check_referrals(name organization, uint64_t min_visitors_invited, uint64_t min_residents_invited);
        void check_status_requirements(name organization, uint64_t status);
        void history_update_org_status(name organization, uint64_t status);
        void calculate_trailing_app_use(const name & appname, const uint64_t & cutoff, const int64_t & threshold);
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<organization>(name(receiver), name(code), &organization::deposit);
  } else if (code == receiver) {
      switch (action) {
          EOSIO_DISPATCH_HELPER(organization, (reset)(addmember)(removemember)(changerole)(changeowner)(addregen)
            (subregen)(create)(destroy)(refund)
            (appuse)(registerapp)(banapp)(calcmappuses)(calcmappuse)(rankappuses)(rankappuse)
            (rankregens)(rankregen)(scoreorgs)(scoretrxs)
            (makethrivble)(makeregen)(makesustnble)(makereptable)(testregensc)(teststatus))
      }
  }
}

