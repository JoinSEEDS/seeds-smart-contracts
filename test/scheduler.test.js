const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows, initContracts } = require('../scripts/helper')
const { equals, init } = require('ramda')

const { scheduler, settings, organization, harvest, accounts, firstuser, token, forum, onboarding, history } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var contracts = null

async function testOperations (operations, assert) {
    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset on mainnet or testnet")
        return
    }

    contracts = await Promise.all([
        eos.contract(scheduler),
        eos.contract(settings),
        eos.contract(accounts),
        eos.contract(organization),
        eos.contract(token)
    ]).then(([scheduler, settings, accounts, organization, token]) => ({
        scheduler, settings, accounts, organization, token
    }))

    let eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
    
    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    const opTable = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'operations',
        limit: 200,
        json: true
    })

    for (const op of opTable.rows) {
        await contracts.scheduler.removeop(op.id, { authorization: `${scheduler}@active` })
        await sleep(200)
    }

    console.log('add operations')
    for (const op of operations) {
        await contracts.scheduler.configop(op.id, op.operation, op.contract, 1, 0, { authorization: `${scheduler}@active` })
        // await sleep(200)
    }
    
    console.log('scheduler execute')
    let canExecute = false
    try {
        for(const op of operations) {
            console.log('to execute:', op.operation)
            await contracts.scheduler.execute({ authorization: `${scheduler}@active` })
            await sleep(300)
            await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )
            await sleep(300)
            await contracts.scheduler.configop(op.id, op.operation, op.contract, 200, 0, { authorization: `${scheduler}@active` })
        }
        canExecute = true
    } catch (error) {
        console.log(error)
        console.log('can not execute (unexpected, permission may be needed)')
        
    }
    assert({
        given: 'called execute',
        should: 'be able to execute organization scores actions',
        actual: canExecute,
        expected: true
    })

    await sleep(1 * 1000)

    await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )
    await sleep(200)
}


describe('scheduler', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset on mainnet or testnet")
        return
    }

    contracts = await Promise.all([
        eos.contract(scheduler),
        eos.contract(settings)
    ]).then(([scheduler, settings]) => ({
        scheduler, settings
    }))

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('configure')
    await contracts.settings.configure('secndstoexec', 1, { authorization: `${settings}@active` })

    console.log('add operations')
    await contracts.scheduler.configop('one', 'test1', 'cycle.seeds', 1, 0, { authorization: `${scheduler}@active` })
    await contracts.scheduler.configop('two', 'test2', 'cycle.seeds', 7, 0, { authorization: `${scheduler}@active` })

    console.log('init test 1')
    await contracts.scheduler.test1({ authorization: `${scheduler}@active` })

    console.log('init test 2')
    await contracts.scheduler.test2({ authorization: `${scheduler}@active` })

    const beforeValues = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    const opTable = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'operations',
        limit: 200,
        json: true
    })

    console.log("before "+JSON.stringify(beforeValues, null, 2))
    for (const op of opTable.rows) {
        await contracts.scheduler.removeop(op.id, { authorization: `${scheduler}@active` })
        await sleep(200)
    }

    const operations = [
        {
            id: 'hrvst.trx',
            operation: 'calctrxpts',
            contract: harvest
        },
        {
            id: 'hrvst.qevs',
            operation: 'calcmqevs',
            contract: harvest
        },
        {
            id: 'hrvst.mintr',
            operation: 'calcmintrate',
            contract: harvest
        },
        {
            id: 'hrvst.hrvst',
            operation: 'runharvest',
            contract: harvest
        },
        {
            id: 'acct.rorgrep',
            operation: 'rankorgreps',
            contract: accounts
        },
        {
            id: 'acct.rorgcbs',
            operation: 'rankorgcbss',
            contract: accounts
        },
        {
            id: 'hrvst.rorgcs',
            operation: 'rankorgcss',
            contract: harvest
        },
        {
            id: 'hstry.ptrxs',
            operation: 'cleanptrxs',
            contract: history
        }
    ]

    console.log('scheduler execute')
    await contracts.scheduler.start( { authorization: `${scheduler}@active` } )

    await sleep(30 * 1000)

    await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )

    const afterValues = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    await sleep(5 * 1000)

    const afterValues2 = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    console.log("after "+JSON.stringify(afterValues, null, 2))

    let delta1 = afterValues.rows[0].value - beforeValues.rows[0].value
    let delta2 = afterValues.rows[1].value - beforeValues.rows[1].value

    // TODO: test pause op

    // TODO: test remove op

    // TODO: test change op to different op

    assert({
        given: '1 second delay was executed 30 seonds',
        should: 'be executed close to 30 times (was: '+delta1+')',
        actual: delta1 >= 20 && delta1 <= 28, // NOTE: ACTUALLY 27 => 31 - 4, 4 is the other action
        expected: true
    })

    assert({
        given: '7 second delay was executed 30 seconds',
        should: 'be executed 4 times',
        actual: delta2,
        expected: 4
    })

    assert({
        given: 'stopped',
        should: 'no more executions',
        actual: [afterValues.rows[0].value, afterValues.rows[1].value],
        expected: [afterValues2.rows[0].value, afterValues2.rows[1].value],
    })

})


describe('scheduler, moon phases', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ scheduler, settings })

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('configure seconds to execute')
    await contracts.settings.configure('secndstoexec', 1, { authorization: `${settings}@active` })

    const opTable = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'operations',
        json: true,
        limit: 1000
    })

    for (const op of opTable.rows) {
        await contracts.scheduler.removeop(op.id, { authorization: `${scheduler}@active` })
    }

    const moonopTable = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'moonops',
        json: true,
        limit: 1000
    })

    for (const op of moonopTable.rows) {
        await contracts.scheduler.removeop(op.id, { authorization: `${scheduler}@active` })
    }

    console.log('populate moonphases')
    let dateTimestamp = parseInt(Date.now() / 1000) + 1
    for (let i = 0; i < 50; i++) {
        await contracts.scheduler.moonphase(dateTimestamp + i, `phase ${(i % 4) + 1}`, '', { authorization: `${scheduler}@active` })
    }

    await contracts.scheduler.configmoonop('one', 'test1', 'cycle.seeds', 1, dateTimestamp, { authorization: `${scheduler}@active` })
    await contracts.scheduler.configmoonop('two', 'test2', 'cycle.seeds', 3, dateTimestamp, { authorization: `${scheduler}@active` })

    console.log('init test 1')
    await contracts.scheduler.test1({ authorization: `${scheduler}@active` })

    console.log('init test 2')
    await contracts.scheduler.test2({ authorization: `${scheduler}@active` })

    const beforeValues = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    console.log("before "+JSON.stringify(beforeValues, null, 2))

    console.log('scheduler execute')
    await contracts.scheduler.start( { authorization: `${scheduler}@active` } )

    await sleep(30 * 1000)

    await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )

    const afterValues = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    await sleep(5 * 1000)

    const afterValues2 = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'test',
        json: true, 
        lower_bound: 'unit.test.1',
        upper_bound: 'unit.test.2',    
        limit: 100
    })

    console.log("after "+JSON.stringify(afterValues, null, 2))

    let delta1 = afterValues.rows[0].value - beforeValues.rows[0].value
    let delta2 = afterValues.rows[1].value - beforeValues.rows[1].value

    console.log('delta1:', delta1)
    console.log('delta2:', delta2)

    const opTable2 = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'operations',
        json: true
    })
    console.log(opTable2)

    const moonopTable2 = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'moonops',
        json: true,
        limit: 1000
    })
    console.log(moonopTable2)

    assert({
        given: '1 second delay was executed 30 seonds',
        should: 'be executed close to 30 times (was: '+delta1+')',
        actual: delta1 >= 20 && delta1 <= 28, // NOTE: ACTUALLY 27 => 31 - 4, 4 is the other action
        expected: true
    })

    assert({
        given: 'test2 executed every 3 moonphases',
        should: 'be executed 8 times',
        actual: delta2,
        expected: 8
    })

    assert({
        given: 'stopped',
        should: 'no more executions',
        actual: [afterValues.rows[0].value, afterValues.rows[1].value],
        expected: [afterValues2.rows[0].value, afterValues2.rows[1].value],
    })

})

describe('scheduler, organization', async assert => {

    await testOperations([
        {
            id: 'org.rankrgen',
            operation: 'rankregens',
            contract: organization
        },
        {
            id: 'org.rankcbs',
            operation: 'rankcbsorgs',
            contract: organization
        },
        {
            id: 'org.screorgs',
            operation: 'scoretrxs',
            contract: organization
        },
        {
            id: 'hrvst.orgtx',
            operation: 'rankorgtxs',
            contract: harvest
        },
        {
            id: 'org.appuses',
            operation: 'calcmappuses',
            contract: organization
        },
        {
            id: 'org.rankapps',
            operation: 'rankappuses',
            contract: organization
        },
        {
            id: 'org.clndaus',
            operation: 'cleandaus',
            contract: organization
        }
    ], assert)

})

describe('scheduler, forum', async assert => {

    await testOperations([
        {
            id: 'forum.rank',
            operation: 'rankforums',
            contract: forum
        },
        {
            id: 'forum.give',
            operation: 'givereps',
            contract: forum
        }
    ], assert)

})

describe('scheduler, harvest', async assert => {

    await testOperations([
        {
            id: 'hrvst.trx',
            operation: 'calctrxpts',
            contract: harvest
        },
        {
            id: 'hrvst.qevs',
            operation: 'calcmqevs',
            contract: harvest
        },
        {
            id: 'hrvst.mintr',
            operation: 'calcmintrate',
            contract: harvest
        },
        {
            id: 'hrvst.hrvst',
            operation: 'runharvest',
            contract: harvest
        },
        {
            id: 'acct.rorgrep',
            operation: 'rankorgreps',
            contract: accounts
        },
        {
            id: 'acct.rorgcbs',
            operation: 'rankorgcbss',
            contract: accounts
        },
        {
            id: 'hrvst.rorgcs',
            operation: 'rankorgcss',
            contract: harvest
        }
    ], assert)

})

describe('scheduler, token', async assert => {

    await testOperations([
        {
            id: 'tokn.resetw',
            operation: 'resetweekly',
            contract: token
        }
    ], assert)

})

describe('scheduler, onboarding', async assert => {

    await testOperations([
        {
            id: 'onbrd.clean',
            operation: 'chkcleanup',
            contract: onboarding
        }
    ], assert)

})

