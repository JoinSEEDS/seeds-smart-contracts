const { describe } = require('riteway')
const { eos, names, isLocal, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda')

const { scheduler, settings, firstuser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var contracts = null

describe.only('scheduler', async assert => {

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
    await contracts.scheduler.configop('one', 'test1', 'schdlr.seeds', 1, { authorization: `${scheduler}@active` })
    await contracts.scheduler.configop('two', 'test2', 'schdlr.seeds', 7, { authorization: `${scheduler}@active` })

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
    await contracts.scheduler.execute( { authorization: `${scheduler}@active` } )

    await sleep(30 * 1000)

    const afterValues = await getTableRows({
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
        expected: 4
    })

})




