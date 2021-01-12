#include <seeds.forum.hpp>
#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>
#include <contracts.hpp>
#include <string>


void forum::size_change(name id, int delta) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = delta;
    });
  } else {
    uint64_t newsize = sitr->size + delta; 
    if (delta < 0) {
      if (sitr->size < -delta) {
        newsize = 0;
      }
    }
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

uint64_t forum::get_size(name id) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    return 0;
  } else {
    return sitr->size;
  }
}

void forum::size_set(name id, uint64_t newsize) {
  auto sitr = sizes.find(id.value);
  if (sitr == sizes.end()) {
    sizes.emplace(_self, [&](auto& item) {
      item.id = id;
      item.size = newsize;
    });
  } else {
    sizes.modify(sitr, _self, [&](auto& item) {
      item.size = newsize;
    });
  }
}

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

    increase_active_users(account);
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

    increase_active_users(account);
    
    return 0;
}


void forum::increase_active_users(name account) {
    auto aitr = actives.find(account.value);
    if (aitr == actives.end()) {
        actives.emplace(_self, [&](auto & active){
            active.account = account;
        });
        size_change(activesize, 1);
    }
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
    while (dayitr != votespower.end()) {
        dayitr = votespower.erase(dayitr);
    }

    auto aitr = actives.begin();
    while (aitr != actives.end()) {
        aitr = actives.erase(aitr);
    }

    auto sitr = sizes.begin();
    while (sitr != sizes.end()) {
        sitr = sizes.erase(sitr);
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
        size_change(repsize, 1);
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
        size_change(repsize, 1);
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
        int64_t points = getpoints(account);
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
        int64_t points = getpoints(account);
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
        int64_t points = getpoints(account);
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
        int64_t points = getpoints(account);
        int exists = vote(account, comment_id, post_id, comment_id, -1 * points);
    }
}


ACTION forum::onperiod() {
    require_auth(permission_level(contracts::forum, "execute"_n));

    auto ditr = config.get(depreciation.value, "Depreciation factor is not configured.");
    uint64_t depreciation = ditr.value;

    auto itr = forumreps.begin();
    while(itr != forumreps.end()){
        forumreps.modify(itr, _self, [&](auto& new_frep) {
            new_frep.reputation *= depreciation / 10000.0;
        });
        itr++;
    }
}


ACTION forum::newday() {
    require_auth(permission_level(contracts::forum, "execute"_n));

    auto itr = votespower.begin();
    while(itr != votespower.end()){
        itr = votespower.erase(itr);
    }
}

ACTION forum::rankforums() {
    uint64_t batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet").value;
    rankforum(0, batch_size, 0);
}

ACTION forum::rankforum(uint64_t start, uint64_t chunksize, uint64_t chunk) {
    require_auth(get_self());

    uint64_t total = get_size(repsize);
    if (total == 0) return;

    uint64_t current = chunk * chunksize;
    auto forum_rep_by_points = forumreps.get_index<"byrep"_n>();
    auto fitr = start == 0 ? forum_rep_by_points.begin() : forum_rep_by_points.lower_bound(start);
    uint64_t count = 0;

    while (fitr != forum_rep_by_points.end() && count < chunksize) {

        uint64_t rank = utils::rank(current, total);

        forum_rep_by_points.modify(fitr, _self, [&](auto& item) {
            item.rank = rank;
        });

        current++;
        count++;
        fitr++;
    }

    if (fitr != forum_rep_by_points.end()) {
        uint64_t next_value = (fitr -> account).value;
        action next_execution(
            permission_level{get_self(), "active"_n},
            get_self(),
            "rankforum"_n,
            std::make_tuple(next_value, chunk + 1, chunksize)
        );

        transaction tx;
        tx.actions.emplace_back(next_execution);
        tx.delay_sec = 1;
        tx.send(repsize.value, _self);
    }
}

uint64_t forum::get_available_points() {
    uint64_t actives = get_size(activesize);
    if (actives < 1000) {
        return actives;
    } else if (actives < 10000) {
        return 0.5 * actives;
    } else if (actives <= 100000) {
        return 0.2 * actives;
    }
    return 0.1 * actives;
}

ACTION forum::givereps() {
    uint64_t batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet").value;
    uint64_t available_points = get_available_points();
    giverep(0, batch_size, available_points);
    delteactives();
}

ACTION forum::giverep(uint64_t start, uint64_t chunksize, uint64_t available_points) {
    require_auth(get_self());

    // uint64_t max_forum_rep = config.get(name("forum.maxrep").value, "The forum.maxrep parameter has not been initialized yet").value;
    auto fitr = start == 0 ? forumreps.begin() : forumreps.find(start);
    uint64_t count = 0;
    double multiplier = available_points / 4851.0;

    while (fitr != forumreps.end() && count < chunksize) {
        uint64_t rep = std::min(multiplier * fitr -> rank, 10.0);
        print("multiplier = ", multiplier, ", rank = ", fitr -> rank, ", result = ", rep, "\n");
        if (rep > 0) {
            action(
                permission_level(contracts::accounts, "active"_n),
                contracts::accounts,
                "addrep"_n,
                std::make_tuple(fitr -> account, rep)
            ).send();
        }
        fitr++;
        count++;
    }

    if (fitr != forumreps.end()) {
        uint64_t next_value = (fitr -> account).value;
        action next_execution(
            permission_level{get_self(), "active"_n},
            get_self(),
            "giverep"_n,
            std::make_tuple(next_value, chunksize, available_points)
        );

        transaction tx;
        tx.actions.emplace_back(next_execution);
        tx.delay_sec = 1;
        tx.send(activesize.value, _self);
    }
}

ACTION forum::delteactives() {
    uint64_t batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet").value;
    deleteactive(batch_size);
}

ACTION forum::deleteactive(uint64_t chunksize) {
    require_auth(get_self());

    auto aitr = actives.begin();
    uint64_t count = 0;

    while (aitr != actives.end() && count < chunksize) {
        aitr = actives.erase(aitr);
        count++;
    }
    size_change(activesize, -1 * count);

    if (aitr != actives.end()) {
        auto next_value = aitr -> account;
        action next_execution(
            permission_level{get_self(), "active"_n},
            get_self(),
            "deleteactive"_n,
            std::make_tuple(chunksize)
        );

        transaction tx;
        tx.actions.emplace_back(next_execution);
        tx.delay_sec = 1;
        tx.send(next_value.value, _self);
    }
}

ACTION forum::testsize (name id, uint64_t size) {
    require_auth(get_self());
    size_set(id, size);
}

ACTION forum::testrnk (uint64_t rnk) {
   auto r = utils::rank(rnk, 100);
   print("rank "+std::to_string(r));
}

ACTION forum::testapoints () {
    require_auth(get_self());
    check(false, std::to_string(get_available_points()));
}

