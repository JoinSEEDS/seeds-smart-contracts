const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda')

const { scheduler, settings, organization, firstuser, forum } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var contracts = null

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

    // TODO: test pause op

    // TODO: test remove op

    // TODO: test change op to different op

    assert({
        given: '1 second delay was executed 30 seonds',
        should: 'be executed close to 30 times (was: '+delta1+')',
        actual: delta1 >= 26 && delta1 <= 28, // NOTE: ACTUALLY 27 => 31 - 4, 4 is the other action
        expected: true
    })

    assert({
        given: '7 second delay was executed 30 seconds',
        should: 'be executed 4 times',
        actual: delta2,
        expected: 5
    })

    assert({
        given: 'stopped',
        should: 'no more executions',
        actual: [afterValues.rows[0].value, afterValues.rows[1].value],
        expected: [afterValues2.rows[0].value, afterValues2.rows[1].value],
    })

    

})

describe('scheduler, organization.cleandaus', async assert => {

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

    console.log('add operations')
    await contracts.scheduler.configop('org.clndaus', 'cleandaus', organization, 1, 0, { authorization: `${scheduler}@active` })

    console.log('scheduler execute')
    let canExecute = false
    try {
        await contracts.scheduler.execute( { authorization: `${scheduler}@active` } )
        canExecute = true
    } catch (err) {
        console.log('can not execute cleandaus (unexpected, permission may be needed)')
    }

    await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )

    assert({
        given: 'called execute',
        should: 'be able to execute cleandaus',
        actual: canExecute,
        expected: true
    })

    await sleep(1 * 1000)

})

describe('scheduler, token.resetweekly', async assert => {

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('add operations')
    await contracts.scheduler.configop('tokn.resetw', 'resetweekly', 'token.seeds', 1, 0, { authorization: `${scheduler}@active` })

    console.log('scheduler execute')
    let canExecute = false
    try {
        await contracts.scheduler.execute({ authorization: `${scheduler}@active` })
        canExecute = true
    } catch (error) {
        console.log('can not execute resetweekly (unexpected, permission may be needed)')
    }
    
    assert({
        given: 'called execute',
        should: 'be able to execute resetweekly',
        actual: canExecute,
        expected: true
    })

    await sleep(1 * 1000)

    await contracts.scheduler.stop( { authorization: `${scheduler}@active` } )

})


describe('scheduler, organization scores', async assert => {

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

    const opTable = await getTableRows({
        code: scheduler,
        scope: scheduler,
        table: 'operations',
        json: true
    })

    for (const op of opTable.rows) {
        await contracts.scheduler.removeop(op.id, { authorization: `${scheduler}@active` })
        await sleep(200)
    }

    const operations = [
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
    ]

    console.log('add operations')
    for (const op of operations) {
        await contracts.scheduler.configop(op.id, op.operation, op.contract, 1, 0, { authorization: `${scheduler}@active` })
        await sleep(200)
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

    console.log('scheduler reset')
    await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

})
