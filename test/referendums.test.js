const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, isLocal } = require('../scripts/helper')

const { referendums, token, settings, accounts, firstuser, seconduser } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

describe('Referendums', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ referendums, token, settings, accounts })

  const stake_price = '1111.0000 SEEDS'
  const favour = 4
  const against = 1
  const referendumId = 0
  const failedReferendumId = 1

  const settingName = 'tempsetting'
  const settingDescription = 'test setting for referendums'
  const settingImpact = 'high'
  const settingValue = 21
  const settingInitialValue = 0

  const addVoice = () => async () => {
    console.log(`add ${favour} voice to ${firstuser}`)
    await contracts.referendums.addvoice(firstuser, favour, { authorization: `${referendums}@active` })

    console.log(`add ${against} voice to ${seconduser}`)
    await contracts.referendums.addvoice(seconduser, against, { authorization: `${referendums}@active` })
  }

  const sendVotes = () => async () => {
    console.log('set quorum to 80 for high impact')
    await contracts.settings.configure('quorum.high', 20, { authorization: `${settings}@active` })

    console.log(`send favour vote from ${firstuser} with value of ${favour} for #${referendumId}`)
    await contracts.referendums.favour(firstuser, referendumId, favour, { authorization: `${firstuser}@active` })

    console.log(`send against vote from ${seconduser} with value of ${against} for #${referendumId}`)
    await contracts.referendums.against(seconduser, referendumId, against, { authorization: `${seconduser}@active` })
    
    console.log(`send favour vote from ${firstuser} with value of ${favour} for #${failedReferendumId}`)
    await contracts.referendums.favour(firstuser, failedReferendumId, favour, { authorization: `${firstuser}@active` })
    
    console.log(`send against vote from 2 with value of ${favour} for #${failedReferendumId}`)
    await contracts.referendums.addvoice(seconduser, favour, { authorization: `${referendums}@active` })
    await contracts.referendums.against(seconduser, failedReferendumId, favour, { authorization: `${seconduser}@active` })
  }

  const executeReferendums = () => async () => {
    console.log(`execute referendums`)
    await sleep(1000)
    await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  }

  const stateHistory = (() => {
    const history = {}

    const getReferendums = async (status) => {
      const referendumsTable = await getTableRows({
        code: referendums,
        scope: status,
        table: 'referendums',
        json: true
      })

      referendumsTable.rows.forEach(row => {
        delete row.created_at
      })

      return referendumsTable
    }

    const save = async (key) => {
      const stagedTable = await getReferendums('staged')
      const activeTable = await getReferendums('active')
      const testingTable = await getReferendums('testing')
      const passedTable = await getReferendums('passed')
      const failedTable = await getReferendums('failed')

      const settingsTable = await getTableRows({
        code: settings,
        scope: settings,
        table: 'config',
        lower_bound: settingName,
        upper_bound: settingName,  
        json: true
      })

      const balancesTable = await getTableRows({
        code: referendums,
        scope: referendums,
        table: 'balances',
        json: true
      })

      const tables = {
        balances: balancesTable,
        settings: settingsTable,
        staged: stagedTable,
        active: activeTable,
        testing: testingTable,
        passed: passedTable,
        failed: failedTable
      }

      history[key] = tables
    }

    const find = (query) => {
      const [table, key] = query.split(':')

      let rows = []

      try {
        rows = history[key][table].rows
      } catch (err) {
        console.log(`cannot find ${query}`)
      }

      return rows
    }

    return {
      save, find
    }
  })()

  const saveState = stateHistory.save
  const table = stateHistory.find

  const metadata = ['title', 'summary', 'description', 'image', 'url']

  const createReferendums = () => async () => {
    console.log(`create referendum from ${firstuser} to change ${settingName} to ${settingValue}`)
    await contracts.referendums.create(firstuser, settingName, settingValue, ...metadata, { authorization: `${firstuser}@active` })

    console.log(`create referendum from ${seconduser} to change ${settingName} to ${settingValue}`)
    await contracts.referendums.create(seconduser, settingName, settingValue, ...metadata, { authorization: `${seconduser}@active` })
  }
  
  const updateReferendum = () => async () => {
    const updatedMetadata = metadata.map(item => item === 'title' ? 'title2' : item)
    
    console.log(`update title of referendum ${settingName}`)
    await contracts.referendums.update(firstuser, settingName, settingValue, ...updatedMetadata, { authorization: `${firstuser}@active` })
  }

  const stake = () => async () => {
    console.log(`increase stake of ${firstuser} to ${stake_price}`)
    await contracts.token.transfer(firstuser, referendums, stake_price, '', { authorization: `${firstuser}@active` })
    
    console.log(`increase stake of ${seconduser} to ${stake_price}`)
    await contracts.token.transfer(seconduser, referendums, stake_price, '', { authorization: `${seconduser}@active` })
  }

  const reset = () => async () => {
    console.log('referendums reset')
    await contracts.referendums.reset({ authorization: `${referendums}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('settings configure')
    await contracts.settings.confwithdesc(settingName, settingInitialValue, settingDescription, settingImpact, { authorization: `${settings}@active` })
    console.log('set stake price to 1')
    //await contracts.settings.configure('refsnewprice', 10000, { authorization: `${settings}@active` })

  }
  const addUsers = () => async () => {
    console.log('add users')
    await contracts.accounts.adduser(firstuser, '1', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
    await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  }

  const fail = (fn) => async () => {
    try {
      await fn()
    } catch (err) {
      console.log('transaction failed as expected')
    }
  }

  const cancelVote = () => async () => {
    console.log(`cancel vote of ${seconduser} for #${referendumId}`)
    await contracts.referendums.cancelvote(seconduser, referendumId, { authorization: `${seconduser}@active` })
  }

  const runTransactions = async (fns) => {
    const keys = Object.keys(fns)

    for (let i = 0; i < keys.length; i++) {
      const fn = fns[keys[i]]

      console.log("running "+keys[i])

      await fn()
      await saveState(keys[i])
    }
  }

  const referendumRow = (referendumId, creatorName) => ({
    referendum_id: referendumId,
    setting_name: settingName,
    setting_value: settingValue,
    creator: creatorName,
    staked: stake_price,
    favour: 0,
    against: 0,
    title: 'title',
    summary: 'summary',
    description: 'description',
    image: 'image',
    url: 'url',
  })

  await runTransactions({
    'reset': reset(),
    'addUsers': addUsers(),
    'addVoice': addVoice("addvoice1"),
    'failedReferendum': fail(createReferendums()),
    'stake': stake(),
    'createReferendums': createReferendums(),
    'updateReferendum': updateReferendum(),
    'failedSendVotes': fail(sendVotes("send1")),
    'executeReferendumsActive': executeReferendums("A"),
    'addVoice-2': addVoice("addvoice2"),
    'addVoice-3': addVoice("addvoice3"),
    'sendVotes': sendVotes(),
    'executeReferendumsTesting': executeReferendums("B"),
    'cancelVote': cancelVote(),
    'executeReferendumsFinal': executeReferendums("C"),
  })

console.log("testing: "+JSON.stringify(table('testing:executeReferendumsTesting')))

  assert({
    given: 'initial referendums table',
    should: 'have no rows',
    actual: table('referendums:reset'),
    expected: []
  })

  assert({
    given: 'initial balances table',
    should: 'have no rows',
    actual: table('balances:reset'),
    expected: []
  })

  assert({
    given: 'initial settings table',
    should: 'have initial value',
    actual: table('settings:reset').find(row => row.param === settingName),
    expected: {
      param: settingName,
      value: settingInitialValue,
      description: settingDescription,
      impact: settingImpact
    }
  })

  assert({
    given: 'balances table after staked seeds',
    should: 'have positive user balance',
    actual: table('balances:stake').find(row => row.account === firstuser),
    expected: {
      account: firstuser,
      stake: stake_price,
      voice: favour
    }
  })

  assert({
    given: 'referendums table after created referendums',
    should: 'have referendums with staged status',
    actual: table('staged:createReferendums'),
    expected: [
      referendumRow(referendumId, firstuser),
      referendumRow(failedReferendumId, seconduser)
    ]
  })
  
  assert({
    given: 'referendums table after updated referendum',
    should: 'have referendum with updated title',
    actual: table('staged:updateReferendum'),
    expected: [
      { ...referendumRow(referendumId, firstuser), title: 'title2' },
      referendumRow(failedReferendumId, seconduser)
    ]
  })

  assert({
    given: 'referendums table after first execution',
    should: 'have referendums with active status',
    actual: table('active:executeReferendumsActive'),
    expected: [
      { ...referendumRow(referendumId, firstuser), title: 'title2' },
      referendumRow(failedReferendumId, seconduser)
    ]
  })

  assert({
    given: 'referendums table after second execution',
    should: 'have referendums with testing status',
    actual: table('testing:executeReferendumsTesting')[0],
    expected: {
      ...referendumRow(referendumId, firstuser),
      against,
      favour,
      title: 'title2'
    }
  })

  assert({
    given: 'referendums table after third execution',
    should: 'have referendums with passed status',
    actual: table('passed:executeReferendumsFinal')[0],
    expected: {
      ...referendumRow(referendumId, firstuser),
      favour,
      against: 0,
      title: 'title2'
    }
  })

  assert({
    given: 'referendums table after third execution',
    should: 'have referendum with failed status',
    actual: table('failed:executeReferendumsFinal'),
    expected: [{
      ...referendumRow(failedReferendumId, seconduser),
      favour,
      against: favour,
    }]
  })

  assert({
    given: 'settings table after second execution',
    should: 'have initial value',
    actual: table('settings:executeReferendumsTesting').find(row => row.param === settingName),
    expected: {
      param: settingName,
      value: settingInitialValue,
      description: settingDescription,
      impact: settingImpact
    }
  })

  assert({
    given: 'settings table after third execution',
    should: 'have value changed to proposed value',
    actual: table('settings:executeReferendumsFinal').find(row => row.param === settingName),
    expected: {
      param: settingName,
      value: settingValue,
      description: settingDescription,
      impact: settingImpact
    }
  })

  assert({
    given: 'balances table after third execution',
    should: 'have updated user balance',
    actual: table('balances:executeReferendumsFinal').find(row => row.account === firstuser),
    expected: {
      account: firstuser,
      stake: '0.0000 SEEDS',
      voice: 0
    }
  })
})


describe('Referendums Keys', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ referendums, token, settings, accounts })

  const stake_price = '1111.0000 SEEDS'
  const referendumId = 0
  const failedReferendumId = 1

  const settingName = 'tempsetting'
  const settingDescription = 'test setting for referendums'
  const settingImpact = 'high'
  const settingValue = 21
  const settingInitialValue = 0
  const metadata = ['title', 'summary', 'description', 'image', 'url']

  const getReferendums = async (status) => {
    const referendumsTable = await getTableRows({
      code: referendums,
      scope: status,
      table: 'referendums',
      json: true
    })

    referendumsTable.rows.forEach(row => {
      delete row.created_at
    })

    return referendumsTable
  }

  const getRefs = async() => {
    const staged = await getReferendums("staged")
    const active = await getReferendums("active")
    const testing = await getReferendums('testing')
    const passed = await getReferendums("passed")
    const failed = await getReferendums("failed")
  
    return [staged, active, testing, passed, failed]
  }

  const checkRefs = async (numbers) => {
    
    let nums = await getRefs()
    nums = nums.map(e => e.rows.length)

    console.log("refs: ", nums)

    assert({
      given: 'refs',
      should: 'have the correct statuses',
      actual: nums,
      expected: numbers
    })
  
  }

  console.log('referendums reset')
  await contracts.referendums.reset({ authorization: `${referendums}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('settings configure')
  await contracts.settings.confwithdesc(settingName, settingInitialValue, settingDescription, settingImpact, { authorization: `${settings}@active` })

  console.log('add users, make citizens')
  await contracts.accounts.adduser(firstuser, '1', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })

  const addVoice2 = async (voice) => {
    console.log(`add voice `+voice)
    await contracts.referendums.addvoice(firstuser, voice, { authorization: `${referendums}@active` })
    await contracts.referendums.addvoice(seconduser, voice, { authorization: `${referendums}@active` })
  }
  
  console.log(`add voice`)
  await addVoice2(20)

  console.log('set quorum to 80 for high impact')
  await contracts.settings.configure('quorum.high', 80, { authorization: `${settings}@active` })
  console.log('set stake price to 1')
  await contracts.settings.configure('refsnewprice', 10000, { authorization: `${settings}@active` })

  console.log(`stake`)
  await contracts.token.transfer(firstuser, referendums, "1.0000 SEEDS", '', { authorization: `${firstuser}@active` })
  console.log(`create referendum`)
  await contracts.referendums.create(firstuser, settingName, settingValue, 'R 1', 'summary', 'description', 'image', 'url', { authorization: `${firstuser}@active` })

  await checkRefs([1, 0, 0, 0, 0])

  console.log(`move to active`)
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })

  await checkRefs([0, 1, 0, 0, 0])

  console.log(`add voice 2`)
  await addVoice2(21)

  console.log(`vote 2`)
  await contracts.referendums.favour(firstuser, 0, 8, { authorization: `${firstuser}@active` })
  await contracts.referendums.favour(seconduser, 0, 8, { authorization: `${seconduser}@active` })

  console.log(`stake`)
  await contracts.token.transfer(firstuser, referendums, "1.0000 SEEDS", '', { authorization: `${firstuser}@active` })
  console.log(`create referendum`)
  await contracts.referendums.create(firstuser, settingName, 2, 'REF 2', 'summary', 'description', 'image', 'url', { authorization: `${firstuser}@active` })

  await checkRefs([1, 1, 0, 0, 0])

  console.log(`execute referendum - move to passed, active`)
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  
  // const r1 = await getRefs()

  // console.log("refs "+JSON.stringify(r1, null, 2))

  await checkRefs([0, 1, 1, 0, 0])

  console.log(`add voice 3`)
  await addVoice2(21)

  console.log(`vote 3`)
  await contracts.referendums.favour(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.referendums.favour(seconduser, 1, 8, { authorization: `${seconduser}@active` })

  console.log(`execute referendum 4`)
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })

  await checkRefs([0, 0, 1, 1, 0])

  console.log(`execute referendum 5`)
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })

  await checkRefs([0, 0, 0, 2, 0])

})
