#include <seeds.forum.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>
#include <contracts.hpp>
#include <string>


void forum::createpostcomment(name account, uint64_t post_id, uint64_t backend_id, string url, string body) {
    auto backendid_index = postcomments.get_index<name("backendid")>();
    auto itr = backendid_index.find(backend_id);

    check(itr == backendid_index.end(), "Backend ID already exists.");

    uint64_t id = postcomments.available_primary_key();

    if(id == 0) id += 1;

    postcomments.emplace(_self, [&](auto& new_post) {
        new_post.id = id;
        new_post.parent_id = post_id;
        new_post.backend_id = backend_id;
        new_post.author_name_account = account;
        new_post.url = url;
        new_post.body_content = body;
        new_post.timestamp = eosio::current_time_point().sec_since_epoch();
        new_post.reputation = 0;
    });
}


int forum::vote(name account, uint64_t id, uint64_t post_id, uint64_t comment_id, int64_t points) {

    vote_tables votes(get_self(), id);

    auto itr = votes.find(account.value);
    if(itr != votes.end()) return -1;

    auto postcomtitr = postcomments.get(id, "The post/comment does not exist.");
    
    votes.emplace(_self, [&](auto& new_vote) {
        new_vote.account = account;
        new_vote.timestamp = eosio::current_time_point().sec_since_epoch();
        new_vote.author = postcomtitr.author_name_account;
        new_vote.post_id = post_id;
        new_vote.comment_id = comment_id;
        new_vote.points = points; 
    });

    auto pcit = postcomments.find(id);
    auto fritr = forumreps.find(pcit -> author_name_account.value);
    forumreps.modify(fritr, _self, [&](auto& frep) {
        frep.reputation += points; 
    });
    
    return 0;
}


int64_t forum::pointsfunction(name account, int64_t points_left, uint64_t vbp, uint64_t rep, uint64_t cutoff, uint64_t cutoff_zero){
    auto itr = votespower.find(account.value);

    if(points_left <= 0){
        votespower.modify(itr, _self, [&](auto& entry) {
            entry.num_votes += 1;
        });
        return 0;
    }

    int64_t points = vbp * (rep / 10000.0);
    
    if(points_left <= cutoff){
        int exp = cutoff / points_left;
        points >>= exp;
    }

    if(points <= cutoff_zero){
        points_left = points;
    }
    
    votespower.modify(itr, _self, [&](auto& entry) {
        entry.num_votes += 1;
        entry.points_left = points_left - points;
    });

    return points;
}

int64_t forum::getpoints(name account) {

    auto itr = votespower.find(account.value);
    auto userit = users.get(account.value, "User does not exist.");
    auto vbpitr = config.get(vbp.value, "Vote Base Point value is not configured.");
    auto cutoffitr = config.get(cutoff.value, "Cut off value is not configured.");
    auto cutoffzitr = config.get(cutoffz.value, "Cut off zero value is not configured.");

    if(itr == votespower.end()){
        auto maxpitr = config.get(maxpoints.value, "Max points value is not configured.");
        int64_t max_points = (maxpitr.value) * (vbpitr.value / 10000.0) * (userit.reputation / 10000.0);

        votespower.emplace(_self, [&](auto& new_vote) {
            new_vote.account = account;
            new_vote.num_votes = 0;
            new_vote.points_left = max_points;
            new_vote.max_points = max_points;
        });

        return pointsfunction(account, max_points, vbpitr.value, userit.reputation, cutoffitr.value, cutoffzitr.value);
    }

    return pointsfunction(account, itr -> points_left, vbpitr.value, userit.reputation, cutoffitr.value, cutoffzitr.value);

}


int forum::updatevote(name account, uint64_t id, uint64_t post_id, uint64_t comment_id, int64_t factor){
    vote_tables votes(get_self(), id);

    auto itr = votes.get(account.value, "Vote does not exist.");
    int64_t points = 0;
    uint64_t periods = 0;

    if(factor < 0){
        check(itr.points >= 0, "A user can not downvote twice the same post/comment.");
    }
    else{
        check(itr.points < 0, "A user can not upvote twice the same post/comment.");
    }

    periods = getdperiods(itr.timestamp);
    points = abs(getdpoints(itr.points, periods));

    auto pcit = postcomments.find(id);
    auto fritr = forumreps.find(pcit -> author_name_account.value);
    forumreps.modify(fritr, _self, [&](auto& frmodified) {
        frmodified.reputation += factor * points;
    });

    auto vitr = votes.find(account.value);
    votes.erase(vitr);
    
    vote(account, id, post_id, comment_id, factor * getpoints(account));
    return 0;
}


int64_t forum::getdpoints(int64_t points, uint64_t periods){
    auto ditr = config.get(depreciation.value, "Depreciation factor is not configured.");
    uint64_t total_d = 10000;
    int64_t total = 0;
    for(uint64_t i = 0; i < periods; i++){
        total_d *= ditr.value / 10000.0;
    }

    total = points * (total_d / 10000.0);

    return total;
}


uint64_t forum::getdperiods(uint64_t timestamp) {
    uint64_t t = eosio::current_time_point().sec_since_epoch();
    auto itr = config.get(depreciations.value, "Depreciations value is not configured.");
    uint64_t v = (t - timestamp) / itr.value;
    return v;
}


bool forum::isRdyToExec(name operation){
    auto itr = operations.find(operation.value);
    check(itr != operations.end(), "The operation does not exist.");
    uint64_t timestamp = eosio::current_time_point().sec_since_epoch();
    uint64_t periods = 0;

    periods = ((timestamp - itr -> timestamp) * 10000) / itr -> period;


    if(periods > 0) return true;
    
    check(false, std::to_string(periods));
    return false;
}


ACTION forum::reset() {
    require_auth(_self);

    auto pcitr = postcomments.begin();
    while (pcitr != postcomments.end()) {
        pcitr = postcomments.erase(pcitr);
    }

    for(uint64_t i = 0; i < 20; i++){
        vote_tables votes(_self, i);
        auto vitr = votes.begin();
        while (vitr != votes.end()) {
            vitr = votes.erase(vitr);
        }
    }

    auto fritr = forumreps.begin();
    while (fritr != forumreps.end()) {
        fritr = forumreps.erase(fritr);
    }

    auto dayitr = votespower.begin();
    while(dayitr != votespower.end()){
        dayitr = votespower.erase(dayitr);
    }

}


ACTION forum::createpost(name account, uint64_t backend_id, string url, string body) {
    require_auth(account);

    auto user = users.get(account.value, "User does not exist.");
    createpostcomment(account, 0, backend_id, url, body);

    auto repitr = forumreps.find(account.value);

    if(repitr == forumreps.end()){
        forumreps.emplace(_self, [&](auto& new_forum_rep) {
            new_forum_rep.account = account;
            new_forum_rep.reputation = 0;
        });
    }
    
}


ACTION forum::createcomt(name account, uint64_t post_id, uint64_t backend_id, string url, string body){
    require_auth(account);
    
    auto itr = postcomments.get(post_id, "Post does not exist.");
    check(itr.parent_id == 0, "Comments can not be commentted.");
    
    createpostcomment(account, post_id, backend_id, url, body);

    auto repitr = forumreps.find(account.value);

    if(repitr == forumreps.end()){
        forumreps.emplace(_self, [&](auto& new_forum_rep) {
            new_forum_rep.account = account;
            new_forum_rep.reputation = 0;
        });
    }
}


ACTION forum::upvotepost(name account, uint64_t id) {
    require_auth(account);

    vote_tables votes(get_self(), id);

    auto itr = votes.find(account.value);
    if(itr != votes.end()){
        updatevote(account, id, id, 0, 1);
    }
    else{
        int64_t points = getpoints(account); // function to determine how many points to asign
        int exists = vote(account, id, id, 0, points);
    }
}


ACTION forum::upvotecomt(name account, uint64_t post_id, uint64_t comment_id) {
    require_auth(account);

    vote_tables votes(get_self(), comment_id);

    auto itr = votes.find(account.value);
    if(itr != votes.end()){
        updatevote(account, comment_id, post_id, comment_id, 1);
    }
    else{
        int64_t points = getpoints(account);; // function to determine how many points to asign
        int exists = vote(account, comment_id, post_id, comment_id, points);
    }
}


ACTION forum::downvotepost(name account, uint64_t id) {
    require_auth(account);

    vote_tables votes(get_self(), id);

    auto itr = votes.find(account.value);
    if(itr != votes.end()){
        updatevote(account, id, id, 0, -1);
    }
    else{
        int64_t points = getpoints(account);; // function to determine how many points to asign
        int exists = vote(account, id, id, 0, -1 * points);
    }
}


ACTION forum::downvotecomt(name account, uint64_t post_id, uint64_t comment_id) {
    require_auth(account);

    vote_tables votes(get_self(), comment_id);

    auto itr = votes.find(account.value);
    if(itr != votes.end()){
        updatevote(account, comment_id, post_id,comment_id, -1);
    }
    else{
        int64_t points = getpoints(account);; // function to determine how many points to asign
        int exists = vote(account, comment_id, post_id, comment_id, -1 * points);
    }
}


ACTION forum::onperiod() {
    //require_auth(permission_level(contracts::forum, "period"_n));
    //require_auth(permission_level(contracts::scheduler, "scheduled"_n));

    if(!isRdyToExec(name("onperiod"))){
        print("onperiod is not ready to be executed.");
        check(false, "onperiod is not ready to be executed.");
    }

    auto ditr = config.get(depreciation.value, "Depreciation factor is not configured.");
    uint64_t depreciation = ditr.value;

    auto itr = forumreps.begin();
    while(itr != forumreps.end()){
        forumreps.modify(itr, _self, [&](auto& new_frep) {
            new_frep.reputation *= depreciation / 10000.0;
        });
        itr++;
    }

    print("onperiod executed successfully.");

}


ACTION forum::newday() {
    //require_auth(contracts::scheduler);

    if(!isRdyToExec(name("newday"))){
        print("newday is not ready to be executed.");
        check(false, "newday is not ready to be executed.");
    }

    auto itr = votespower.begin();
    while(itr != votespower.end()){
        itr = votespower.erase(itr);
    }

    print("newday executed successfully.");

}


EOSIO_DISPATCH(forum, (createpost)(createcomt)(upvotepost)(upvotecomt)(downvotepost)(downvotecomt)(reset)(onperiod)(newday));
