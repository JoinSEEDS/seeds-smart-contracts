const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda');
const { expect, use } = require('chai');

const { forum, scheduler, accounts, settings, application, firstuser, seconduser, thirduser } = names


function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}


describe('forum', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

     const contracts = await Promise.all([
        eos.contract(forum),
        eos.contract(scheduler),
        eos.contract(accounts),
        eos.contract(settings)
    ]).then(([forum, scheduler, accounts, settings]) => ({
        forum, scheduler, accounts, settings
    }))

    console.log('forum reset')
    await contracts.forum.reset({ authorization: `${forum}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('configure')
    await contracts.settings.configure("maxpoints", 100000, { authorization: `${settings}@active` })
    await contracts.settings.configure("vbp", 70000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoff", 280000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoffz", 5000, { authorization: `${settings}@active` })
    await contracts.settings.configure("depreciation", 9500, { authorization: `${settings}@active` })
    await contracts.settings.configure("dps", 5, { authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

    console.log('add reputation')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 5000, { authorization: `${accounts}@active` })

    console.log('creating posts')
    await contracts.forum.createpost(firstuser, 1, 'url1', 'body1', { authorization: `${firstuser}@active` })
    await contracts.forum.createpost(seconduser, 2, 'url2', 'body2', { authorization: `${seconduser}@active` })
    
    console.log('creating comments')
    await contracts.forum.createcomt(firstuser, 2, 3, 'url3', 'body3', { authorization: `${firstuser}@active` })
    await contracts.forum.createcomt(seconduser, 1, 4, 'url4', 'body4', { authorization: `${seconduser}@active` })

    const postcomments = await getTableRows({
        code: forum,
        scope: forum,
        table: 'postcomment',
        json: true
    })

    console.log('vote post')
    await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })

    try{
        console.log('vote twice')
        await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    }
    catch(err){
        console.log('an user can not upvote twice the same post')
    }


    await contracts.forum.downvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(seconduser, 2, { authorization: `${seconduser}@active` })
    await contracts.forum.upvotepost(firstuser, 1, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(seconduser, 1, { authorization: `${seconduser}@active` })

    try{
        console.log('vote a non existing post')
        await contracts.forum.upvotepost(firstuser, 20, { authorization: `${firstuser}@active` })
    }
    catch(err){
        console.log('the post does not exist')
    }

    console.log('vote comments')
    await contracts.forum.upvotecomt(firstuser, 1, 4, { authorization: `${firstuser}@active` })
    await contracts.forum.upvotecomt(seconduser, 1, 4, { authorization: `${seconduser}@active` })

    const repBeforeDepreciation = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })

    console.log('depreciate')
    await contracts.forum.onperiod({ authorization: `${forum}@execute` })
    await sleep(10000)
    await contracts.forum.onperiod({ authorization: `${forum}@execute` })

    console.log('vote comments')

    try{
        console.log('downvote a non existing comment')
        await contracts.forum.downvotecomt(seconduser, 1, 40, { authorization: `${seconduser}@active` })
    }
    catch(err){
        console.log('the comment does not exist')
    }

    await contracts.forum.downvotecomt(seconduser, 1, 4, { authorization: `${seconduser}@active` })

    const repAfterDepreciation = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })

    console.log('new day')
    try{
        await contracts.forum.newday({ authorization: `${forum}@execute` })
    }
    catch(err){
        console.log("new day is not ready to be executed.", err)
    }

    console.log('vote posts')
    await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.upvotepost(seconduser, 1, { authorization: `${seconduser}@active` })

    const repAfterNewDay = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })

    const votesPower = await getTableRows({
        code: forum,
        scope: forum,
        table: 'votepower',
        json: true
    })
    
    assert({
        given: 'users have voted',
        should: 'update their corresponding vote power',
        actual: votesPower.rows.map(row => {
            return row
        }),
        expected: [
            {
                account: firstuser,
                num_votes: 1,
                points_left: 630000,
                max_points: 700000
                },
                {
                account: seconduser,
                num_votes: 1,
                points_left: 315000,
                max_points: 350000
            }
        ]
    })

    assert({
        given: 'vote posts and comments',
        should: 'update the forum reputation table',
        actual: repBeforeDepreciation.rows.map(row => row.reputation),
        expected: [35000, -17500]
    })

    assert({
        given: 'vote posts and comments after depreciation',
        should: 'update the vote with its correspondent depreciation',
        actual: repAfterDepreciation.rows.map(row => row.reputation),
        expected: [31587, -49086]
    })

    assert({
        given: 'vote posts and comments after a new day',
        should: 'update the vote with its whole vote power',
        actual: repAfterNewDay.rows.map(row => row.reputation),
        expected: [98174, 84089]
    })

    assert({
        given: 'post and comment creater',
        should: 'have the introduced values',
        actual: postcomments.rows.map(row => {
            return {
                id: row.id,
                parent_id: row.parent_id,
                backend_id: row.backend_id,
                author_name_account: row.author_name_account,
                url: row.url,
                body_content: row.body_content,
                reputation: row.reputation
            }
        }),
        expected: [
            {
                id: 1,
                parent_id: 0,
                backend_id: 1,
                author_name_account: firstuser,
                url: 'url1',
                body_content: 'body1',
                reputation: 0
            },
            {
                id: 2,
                parent_id: 0,
                backend_id: 2,
                author_name_account: seconduser,
                url: 'url2',
                body_content: 'body2',
                reputation: 0
            },
            {
                id: 3,
                parent_id: 2,
                backend_id: 3,
                author_name_account: firstuser,
                url: 'url3',
                body_content: 'body3',
                reputation: 0
            },
            {
                id: 4,
                parent_id: 1,
                backend_id: 4,
                author_name_account: seconduser,
                url: 'url4',
                body_content: 'body4',
                reputation: 0
            }
        ]
    })

})


describe('forum reputation', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

     const contracts = await Promise.all([
        eos.contract(forum),
        eos.contract(scheduler),
        eos.contract(accounts),
        eos.contract(settings)
    ]).then(([forum, scheduler, accounts, settings]) => ({
        forum, scheduler, accounts, settings
    }))
    
    console.log('forum reset')
    await contracts.forum.reset({ authorization: `${forum}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(thirduser, 'third user', 'individual', { authorization: `${accounts}@active` })

    console.log('add reputation')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 5000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(thirduser, 10000, { authorization: `${accounts}@active` })

    console.log('creating posts')
    await contracts.forum.createpost(firstuser, 1, 'url1', 'body1', { authorization: `${firstuser}@active` })
    await contracts.forum.createpost(seconduser, 2, 'url2', 'body2', { authorization: `${seconduser}@active` })
    await contracts.forum.createpost(thirduser, 3, 'url3', 'body3', { authorization: `${thirduser}@active` })
    
    console.log('creating comments')
    await contracts.forum.createcomt(firstuser, 2, 5, 'url3', 'body3', { authorization: `${firstuser}@active` })
    await contracts.forum.createcomt(seconduser, 1, 6, 'url4', 'body4', { authorization: `${seconduser}@active` })

    console.log('voting')
    await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.upvotepost(seconduser, 3, { authorization: `${seconduser}@active` })
    await contracts.forum.downvotecomt(thirduser, 2, 4, { authorization: `${thirduser}@active` })

    const testGetPoints = async (expectedValue, size = null) => {
        size = parseInt(size)
        if (!isNaN(size)) {
            await contracts.forum.testsize('active.sz', size, { authorization: `${forum}@active` })
            await sleep(300)
        }
        try {
            await contracts.forum.testapoints({ authorization: `${forum}@active` })
        } catch (e) {            
            const error = JSON.parse(JSON.stringify(e, null, 2))
            assert({
                given: 'test points called',
                should: 'return the correct amount of points',
                expected: `assertion failure with message: ${parseInt(expectedValue)}`,
                actual: error.json.error.details[0].message
            })
        }
    }

    await contracts.forum.rankforums({ authorization: `${forum}@active` })
    await sleep(300)

    const forumReputation = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })

    const sizes = await getTableRows({
        code: forum,
        scope: forum,
        table: 'sizes',
        json: true
    })

    await contracts.forum.rankforums({ authorization: `${forum}@active` })
    await sleep(300)

    await contracts.forum.testsize('active.sz', 500, { authorization: `${forum}@active` })
    await sleep(300)

    await contracts.forum.givereps({ authorization: `${forum}@active` })
    await sleep(300)

    const users = await getTableRows({
        code: accounts,
        scope: accounts,
        table: 'users',
        json: true
    })

    await testGetPoints(497)

    console.log('testing correct available points')
    await testGetPoints(999, 999)
    await testGetPoints(parseInt(0.5 * 1000), 1000)
    await testGetPoints(parseInt(0.2 * 20000), 20000)
    await testGetPoints(parseInt(0.1 * 100001), 100001)

    ranktest = await contracts.forum.testrank(50, { authorization: `${forum}@active` });

    assert({
        given: 'rankforums called',
        should: 'rank all the users in the forum correctly',
        expected: [
            { account: firstuser, reputation: -70000, rank: 0 },
            { account: seconduser, reputation: 70000, rank: 50 },
            { account: thirduser, reputation: 35000, rank: 8 }
        ],
        actual: forumReputation.rows
    })

    assert({
        given: 'rep given in the forum',
        should: 'have correct rep size',
        expected: [ { id: 'active.sz', size: 3 }, { id: 'rep.sz', size: 3 } ],
        actual: sizes.rows
    })

    assert({
        given: 'reputation distributed',
        should: 'give users correct reputation',
        expected: [10000, 5005, 10000],
        actual: users.rows.map(u => u.reputation)
    })

    assert({
        given: 'ranking using spline coeficients',
        should: 'return expected value',
        expected: 'rank 27',
        actual: ranktest.processed.action_traces[0].console
    })
})







