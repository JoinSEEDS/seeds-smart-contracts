const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts } = require('../scripts/helper')

const { referendums, token, settings, accounts, firstuser, seconduser } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

describe('Referendums', async assert => {
  const contracts = await initContracts({ referendums, token, settings, accounts })

  const stake_price = '1000.0000 SEEDS'
  const favour = 8
  const against = 2
  const referendumId = 0

  const settingName = "tempsetting"
  const settingValue = 21
  const settingInitialValue = 0

  const addVoice = async () => {
    console.log(`add ${favour} voice to ${firstuser}`)
    await contracts.referendums.addvoice(firstuser, favour, { authorization: `${referendums}@active` })

    console.log(`add ${against} voice to ${seconduser}`)
    await contracts.referendums.addvoice(seconduser, against, { authorization: `${referendums}@active` })
  }

  const sendVotes = async () => {
    console.log(`send favour vote from ${firstuser} with value of ${favour} for #${referendumId}`)
    await contracts.referendums.favour(firstuser, referendumId, favour, { authorization: `${firstuser}@active` })

    console.log(`send against vote from ${seconduser} with value of ${against} for #${referendumId}`)
    await contracts.referendums.against(seconduser, referendumId, against, { authorization: `${seconduser}@active` })
  }

  const executeReferendums = async () => {
    console.log(`execute referendums`)
    await sleep(1000)
    await contracts.referendums.runcycle({ authorization: `${referendums}@active` })
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

  const createReferendum = async () => {
    console.log(`create referendum from ${firstuser} to change ${settingName} to ${settingValue}`)
    await contracts.referendums.create(firstuser, settingName, settingValue, 'title', 'summary', 'description', 'image', 'url', { authorization: `${firstuser}@active` })
  }

  const stake = async () => {
    console.log(`increase stake of ${firstuser} to ${stake_price}`)
    await contracts.token.transfer(firstuser, referendums, stake_price, '', { authorization: `${firstuser}@active` })
  }

  const reset = async () => {
    console.log('referendums reset')
    await contracts.referendums.reset({ authorization: `${referendums}@active` })

    console.log('settings configure')
    await contracts.settings.configure(settingName, settingInitialValue, { authorization: `${settings}@active` })
  }

  const fail = (fn) => async () => {
    try {
      await fn()
    } catch (err) {
      console.log('transaction failed as expected')
    }
  }

  const cancelVote = async () => {
    console.log(`cancel vote of ${seconduser} for #${referendumId}`)
    await contracts.referendums.cancelvote(seconduser, referendumId, { authorization: `${seconduser}@active` })
  }

  const runTransactions = async (fns) => {
    const keys = Object.keys(fns)

    for (let i = 0; i < keys.length; i++) {
      const fn = fns[keys[i]]

      await fn()
      await saveState(keys[i])
    }
  }

  const referendumRow = {
    referendum_id: referendumId,
    setting_name: settingName,
    setting_value: settingValue,
    creator: firstuser,
    staked: stake_price,
    favour: 0,
    against: 0,
    title: 'title',
    summary: 'summary',
    description: 'description',
    image: 'image',
    url: 'url',
  }

  await runTransactions({
    'reset': reset,
    'addVoice': addVoice,
    'failedReferendum': fail(createReferendum),
    'stake': stake,
    'createReferendum': createReferendum,
    'failedSendVotes': fail(sendVotes),
    'executeReferendumsActive': executeReferendums,
    'sendVotes': sendVotes,
    'executeReferendumsTesting': executeReferendums,
    'cancelVote': cancelVote,
    'executeReferendumsFinal': executeReferendums
  })

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
      value: settingInitialValue
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
    given: 'referendums table after created referendum',
    should: 'have referendum with staged status',
    actual: table('staged:createReferendum'),
    expected: [{
      ...referendumRow
    }]
  })

  assert({
    given: 'referendums table after first execution',
    should: 'have referendum with active status',
    actual: table('active:executeReferendumsActive'),
    expected: [{
      ...referendumRow
    }]
  })

  assert({
    given: 'referendums table after second execution',
    should: 'have referendums with testing status',
    actual: table('testing:executeReferendumsTesting'),
    expected: [{
      ...referendumRow,
      against,
      favour
    }]
  })

  assert({
    given: 'referendums table after third execution',
    should: 'have referendums with passed status',
    actual: table('passed:executeReferendumsFinal'),
    expected: [{
      ...referendumRow,
      favour,
      against: 0
    }]
  })

  assert({
    given: 'referendums table after third execution',
    should: 'have no referendums with failed status',
    actual: table('failed:executeReferendumsFinal'),
    expected: []
  })

  assert({
    given: 'settings table after second execution',
    should: 'have initial value',
    actual: table('settings:executeReferendumsTesting').find(row => row.param === settingName),
    expected: {
      param: settingName,
      value: settingInitialValue
    }
  })

  assert({
    given: 'settings table after third execution',
    should: 'have value changed to proposed value',
    actual: table('settings:executeReferendumsFinal').find(row => row.param === settingName),
    expected: {
      param: settingName,
      value: settingValue
    }
  })

  assert({
    given: 'balances table after third execution',
    should: 'have updated user balance',
    actual: table('balances:executeReferendumsFinal').find(row => row.account === firstuser),
    expected: {
      account: firstuser,
      stake: '0.0000 SEEDS',
      voice: 10
    }
  })
})
