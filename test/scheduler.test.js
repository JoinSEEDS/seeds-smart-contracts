const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda')

const { scheduler, forum, accounts, settings, application, firstuser, seconduser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var contracts = null

describe('scheduler', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    contracts = await Promise.all([
        eos.contract(scheduler),
        eos.contract(forum),
        eos.contract(accounts),
        eos.contract(settings)
    ]).then(([scheduler, forum, accounts, settings]) => ({
        scheduler, forum, accounts, settings
    }))

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('forum reset')
    await contracts.forum.reset({ authorization: `${forum}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('configure')
    await contracts.scheduler.configop('onperiod', 'forum.seeds', 7, { authorization: `${scheduler}@active` })
    await contracts.scheduler.configop('newday', 'forum.seeds', 15, { authorization: `${scheduler}@active` })
    await contracts.settings.configure("maxpoints", 100000, { authorization: `${settings}@active` })
    await contracts.settings.configure("vbp", 70000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoff", 280000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoffz", 5000, { authorization: `${settings}@active` })
    await contracts.settings.configure("depreciation", 9500, { authorization: `${settings}@active` })
    await contracts.settings.configure("dps", 2, { authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

    console.log('add reputation')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 5000, { authorization: `${accounts}@active` })

    console.log('creating posts')
    await contracts.forum.createpost(firstuser, 1, 'url1', 'body1', { authorization: `${firstuser}@active` })
    await contracts.forum.createpost(seconduser, 2, 'url2', 'body2', { authorization: `${seconduser}@active` })

    console.log('vote post')
    await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(seconduser, 1, { authorization: `${seconduser}@active` })

    await sleep(30)
    console.log('scheduler execute')
    await contracts.scheduler.execute([], { authorization: `${scheduler}@active` })

    const repBeforeDepreciation = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })

    await sleep(10000)
    console.log('scheduler execute')
    await contracts.scheduler.execute([], { authorization: `${scheduler}@active` })


    const repAfterDepreciation = await getTableRows({
        code: forum,
        scope: forum,
        table: 'forumrep',
        json: true
    })


    console.log('spend vote power')
    for(let i = 0; i < 16; i++) {
        if(i % 2 == 0){
            await contracts.forum.downvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
            await contracts.forum.upvotepost(seconduser, 1, { authorization: `${seconduser}@active` })
        }
        else{
            await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
            await contracts.forum.downvotepost(seconduser, 1, { authorization: `${seconduser}@active` })
        }
    }

    const votePowerBeforeNewDay = await getTableRows({
        code: forum,
        scope: forum,
        table: 'votepower',
        json: true
    })

    await sleep(6000)
    console.log('scheduler execute')
    await contracts.scheduler.execute([], { authorization: `${scheduler}@active` })

    console.log('vote post')
    await contracts.forum.downvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.upvotepost(seconduser, 1, { authorization: `${seconduser}@active` })

    const votePowerAfterNewDay = await getTableRows({
        code: forum,
        scope: forum,
        table: 'votepower',
        json: true
    })

    assert({
        given: 'the function onperiod not ready to be executed',
        should: 'not execute the function onperiod',
        actual: repBeforeDepreciation.rows.map(row => row),
        expected: [
            { account: 'seedsuseraaa', reputation: -35000 },
            { account: 'seedsuserbbb', reputation: 70000 }
        ]
    })

    assert({
        given: 'the function onperiod ready to be executed',
        should: 'execute the function onperiod',
        actual: repAfterDepreciation.rows.map(row => row),
        expected: [
            { account: 'seedsuseraaa', reputation: -33250 },
            { account: 'seedsuserbbb', reputation: 66500 }
        ]
    })

    assert({
        given: 'the function newday not ready to be executed',
        should: 'not execute the function newday',
        actual: votePowerBeforeNewDay.rows.map(row => row),
        expected: [
            {
                account: 'seedsuseraaa',
                num_votes: 17,
                points_left: 0,
                max_points: 700000
            },
            {
                account: 'seedsuserbbb',
                num_votes: 17,
                points_left: 0,
                max_points: 350000
            }
        ]
    })

    assert({
        given: 'the function newday ready to be executed',
        should: 'execute the function newday',
        actual: votePowerAfterNewDay.rows.map(row => row),
        expected: [
            {
                account: 'seedsuseraaa',
                num_votes: 1,
                points_left: 630000,
                max_points: 700000
            },
            {
                account: 'seedsuserbbb',
                num_votes: 1,
                points_left: 315000,
                max_points: 350000
            }
      ]
    })

})





