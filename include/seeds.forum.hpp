#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT forum : public contract {
    public:
        using contract::contract;
        forum(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              postcomments(receiver, receiver.value),
              forum_reps(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value),
              votes_power(receiver, receiver.value)
              {}
        
        ACTION reset();

        ACTION createpost(name account, uint64_t backend_id, string url, string body);

        ACTION createcomt(name account, uint64_t post_id, uint64_t backend_id, string url, string body);

        ACTION upvotepost(name account, uint64_t id);

        ACTION upvotecomt(name account, uint64_t post_id, uint64_t comment_id);

        ACTION downvotepost(name account, uint64_t id);

        ACTION downvotecomt(name account, uint64_t post_id, uint64_t comment_id);

        ACTION onperiod();

        ACTION newday();

    private:
        // symbol seeds_symbol = symbol("SEEDS", 4);

        TABLE postcomment_table {
            uint64_t id;
            uint64_t parent_id;
            uint64_t backend_id;
            name author_name_account;
            string url;
            string body_content;
            uint64_t timestamp;
            int64_t reputation;

            uint64_t primary_key() const { return id; }
            uint64_t get_backend_id() const { return backend_id; }
        };

        TABLE vote_table {
            name account;
            uint64_t timestamp;
            name author;
            uint64_t post_id;
            uint64_t comment_id;
            int64_t points;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE forum_rep_table {
            name account;
            int64_t reputation;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE user_table {
            name account;
            name status;
            name type;
            string nickname;
            string image;
            string story;
            string roles;
            string skills;
            string interests;
            uint64_t reputation;
            uint64_t timestamp;

            uint64_t primary_key() const { return account.value; }
            uint64_t by_reputation() const { return reputation; }
        };

        TABLE vote_power_table {
            name account;
            uint32_t num_votes;
            int64_t points_left;
            int64_t max_points;

            uint64_t primary_key() const { return account.value; }
        };

        TABLE config_table {
            name param;
            uint64_t value;
            uint64_t primary_key() const { return param.value; }
        };

        typedef eosio::multi_index <"postcomment"_n, postcomment_table, 
            indexed_by<"backendid"_n, const_mem_fun < postcomment_table, 
            uint64_t, &postcomment_table::get_backend_id >>> postcomment_tables;

        typedef eosio::multi_index <"vote"_n, vote_table> vote_tables;

        typedef eosio::multi_index <"forumrep"_n, forum_rep_table> forum_rep_tables;

        typedef eosio::multi_index <"users"_n, user_table,
            indexed_by<"byreputation"_n,
            const_mem_fun<user_table, uint64_t, &user_table::by_reputation>>
        > user_tables;

        typedef eosio::multi_index <"votepower"_n, vote_power_table> vote_power_tables;

        typedef eosio::multi_index<"config"_n, config_table> config_tables;

        postcomment_tables postcomments;
        forum_rep_tables forum_reps;
        user_tables users;
        vote_power_tables votes_power;
        config_tables config;

        const name maxpoints = "maxpoints"_n;
        const name vbp = "vbp"_n;
        const name cutoff = "cutoff"_n;
        const name cutoffz = "cutoffz"_n;
        const name depreciation = "depreciation"_n;

        void createpostcomment(name account, uint64_t post_id, uint64_t backend_id, string url, string body);
        int vote(name account, uint64_t id, uint64_t post_id, uint64_t comment_id, int64_t points);
        int updatevote(name account, uint64_t id, int64_t factor);
        int64_t getpoints(name account);
        int64_t pointsfunction(name account, int64_t points_left, uint64_t vbp, uint64_t rep, uint64_t cutoff, uint64_t cutoff_zero);
};









