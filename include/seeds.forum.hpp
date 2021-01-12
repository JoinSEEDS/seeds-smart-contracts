#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <contracts.hpp>
#include <string>
#include <tables/user_table.hpp>
#include <tables/config_table.hpp>
#include <tables/size_table.hpp>
#include <utils.hpp>

using namespace eosio;
using std::string;

CONTRACT forum : public contract {
    public:
        using contract::contract;
        forum(name receiver, name code, datastream<const char*> ds)
            : contract(receiver, code, ds),
              postcomments(receiver, receiver.value),
              forumreps(receiver, receiver.value),
              actives(receiver, receiver.value),
              sizes(receiver, receiver.value),
              users(contracts::accounts, contracts::accounts.value),
              config(contracts::settings, contracts::settings.value),
              votespower(receiver, receiver.value),
              operations(contracts::scheduler, contracts::scheduler.value)
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

        ACTION rankforums();

        ACTION rankforum(uint64_t start, uint64_t chunksize, uint64_t chunk);

        ACTION givereps();

        ACTION giverep(uint64_t start, uint64_t chunksize, uint64_t available_points);

        ACTION delteactives();

        ACTION deleteactive(uint64_t chunksize);



        ACTION testapoints ();
        ACTION testsize (name id, uint64_t size);
        ACTION testrnk (uint64_t rnk); // FOR TESTING - REMOVE

    private:
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
            uint64_t rank;

            uint64_t primary_key() const { return account.value; }
            uint64_t by_reputation() const {
                uint64_t reputation_id = 1;
                reputation_id <<= 63;
                return ((reputation < 0) ? -1 : 0) + reputation_id + reputation;
            }
            uint64_t by_rank() const { return rank; }
        };

        DEFINE_SIZE_TABLE

        DEFINE_SIZE_TABLE_MULTI_INDEX

        DEFINE_USER_TABLE

        DEFINE_USER_TABLE_MULTI_INDEX

        TABLE vote_power_table {
            name account;
            uint32_t num_votes;
            int64_t points_left;
            int64_t max_points;

            uint64_t primary_key() const { return account.value; }
        };

        DEFINE_CONFIG_TABLE
        
        DEFINE_CONFIG_TABLE_MULTI_INDEX

        TABLE operations_table {
            name operation;
            name contract;
            uint64_t period;
            uint64_t timestamp;

            uint64_t primary_key() const { return operation.value; }
        };

        TABLE active_table {
            name account;

            uint64_t primary_key() const { return account.value; }
        };


        typedef eosio::multi_index <"postcomment"_n, postcomment_table, 
            indexed_by<"backendid"_n, const_mem_fun < postcomment_table, 
            uint64_t, &postcomment_table::get_backend_id >>> postcomment_tables;

        typedef eosio::multi_index <"vote"_n, vote_table> vote_tables;

        typedef eosio::multi_index <"forumrep"_n, forum_rep_table,
            indexed_by<"byrep"_n,
            const_mem_fun<forum_rep_table, uint64_t, &forum_rep_table::by_reputation>>,
            indexed_by<"byrank"_n,
            const_mem_fun<forum_rep_table, uint64_t, &forum_rep_table::by_rank>>
        > forum_rep_tables;

        typedef eosio::multi_index <"votepower"_n, vote_power_table> vote_power_tables;

        typedef eosio::multi_index < "operations"_n, operations_table> operations_tables;

        typedef eosio::multi_index <"actives"_n, active_table> active_tables;

        postcomment_tables postcomments;
        forum_rep_tables forumreps;
        user_tables users;
        vote_power_tables votespower;
        config_tables config;
        operations_tables operations;
        active_tables actives;
        size_tables sizes;
        

        // all these values are expected to be configured in settings
        const name maxpoints = "forum.maxp"_n; // max points
        const name vbp = "forum.vpb"_n; // value based points
        const name cutoff = "forum.cutoff"_n; // cut off to start the dilution
        const name cutoffz = "forum.cutzro"_n; // cut off to consider as 0 the points value
        const name depreciation = "forum.dp"_n; // depreciation factor
        const name depreciations = "forum.dps"_n; // the depreciation period in seconds
        const name repsize = "rep.sz"_n;
        const name activesize = "active.sz"_n;


        void createpostcomment(name account, uint64_t post_id, uint64_t backend_id, string url, string body);
        int vote(name account, uint64_t id, uint64_t post_id, uint64_t comment_id, int64_t points);
        int updatevote(name account, uint64_t id, uint64_t post_id, uint64_t comment_id, int64_t factor);
        int64_t getpoints(name account);
        int64_t pointsfunction(name account, int64_t points_left, uint64_t vbp, uint64_t rep, uint64_t cutoff, uint64_t cutoff_zero);
        uint64_t getdperiods(uint64_t timestamp);
        int64_t getdpoints(int64_t points, uint64_t periods);
        void size_change(name id, int delta);
        uint64_t get_size(name id);
        void increase_active_users(name account);
        uint64_t get_available_points();
        void size_set(name id, uint64_t newsize);
};

EOSIO_DISPATCH(forum, 
    (createpost)(createcomt)(upvotepost)(upvotecomt)(downvotepost)(downvotecomt)(reset)(onperiod)(newday)
    (rankforums)(rankforum)(givereps)(giverep)(delteactives)(deleteactive)
    (testapoints)(testsize)(testrnk)
);
