const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda')

const { scheduler, forum, accounts, settings, application, firstuser, seconduser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var contracts = null

describe('forum', async assert => {

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
    await contracts.scheduler.configop('onperiod', 'seedsforumtx', 20000, { authorization: `${scheduler}@active` })
    await contracts.settings.configure("maxpoints", 100000, { authorization: `${settings}@active` })
    await contracts.settings.configure("vbp", 70000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoff", 280000, { authorization: `${settings}@active` })
    await contracts.settings.configure("cutoffz", 5000, { authorization: `${settings}@active` })
    await contracts.settings.configure("depreciation", 9500, { authorization: `${settings}@active` })
    await contracts.settings.configure("dps", 2, { authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', { authorization: `${accounts}@active` })

    console.log('add reputation')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 5000, { authorization: `${accounts}@active` })

    console.log('creating posts')
    await contracts.forum.createpost(firstuser, 1, 'url1', 'body1', { authorization: `${firstuser}@active` })
    await contracts.forum.createpost(seconduser, 2, 'url2', 'body2', { authorization: `${seconduser}@active` })

    console.log('vote post')
    await contracts.forum.upvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(firstuser, 2, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(seconduser, 2, { authorization: `${seconduser}@active` })
    await contracts.forum.upvotepost(firstuser, 1, { authorization: `${firstuser}@active` })
    await contracts.forum.downvotepost(seconduser, 1, { authorization: `${seconduser}@active` })

    console.log('scheduler execute')
    await contracts.scheduler.execute([], { authorization: `${scheduler}@active` })
    await sleep(5000)
    console.log('scheduler execute')
    await contracts.scheduler.execute([], { authorization: `${scheduler}@active` })
})





