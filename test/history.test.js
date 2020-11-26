const { describe } = require("riteway")
const { names, getTableRows, isLocal, initContracts } = require("../scripts/helper")
const eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

const { firstuser, seconduser, history, accounts, organization, token, settings } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe('make a transaction entry', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ history, accounts })
  
  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })  

  console.log('add transaction entry')
  await contracts.history.trxentry(firstuser, seconduser, '10.0000 SEEDS', { authorization: `${history}@active` })
  
  const { rows } = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'transactions',
    json: true
  })
  
console.log("transactions result "+JSON.stringify(rows, null, 2))

  let txresult = rows[0]
  delete txresult.timestamp

  assert({
    given: 'transactions table',
    should: 'have transaction entry',
    actual: txresult,
    expected: {
      id: 0,
      to: seconduser,
      quantity: '10.0000 SEEDS',
    }
  })
})

describe("make a history entry", async (assert) => {

    const contracts = await initContracts({ history })

    console.log('history reset')
    await contracts.history.reset(firstuser, { authorization: `${history}@active` })
    
    console.log('history make entry')
    await contracts.history.historyentry(firstuser, "tracktest", 77,  "vasily", { authorization: `${history}@active` })
    var txTime = parseInt(Math.round(new Date()/1000) / 100)
    console.log("now time "+txTime)

    console.log('check that history table has the entry')
    
    const { rows } = await getTableRows({
        code: history,
        scope: firstuser,
        table: "history",
        json: true
    })

    let timestamp = parseInt(rows[0].timestamp / 100)

    console.log("timestamp "+parseInt(rows[0].timestamp))

    let rowWithoutTimestamp = rows[0]
    delete rowWithoutTimestamp.timestamp
    assert({ 
        given: "action was tracked",
        should: "have table entry",
        actual: rowWithoutTimestamp,
        expected: {
            history_id: 0,
            account: firstuser,
            action: "tracktest",
            amount: 77,
            meta: "vasily",
        }
    })


    assert({ 
        given: "action was tracked",
        should: "timestamp is kinda close",
        actual: timestamp,
        expected: txTime,
    })

  console.log('add resident')
  await contracts.history.addresident(firstuser, { authorization: `${history}@active` })
  
  console.log('add citizen')
  await contracts.history.addcitizen(seconduser, { authorization: `${history}@active` })

  const residents = await getTableRows({
    code: history,
    scope: history,
    table: 'residents',
    json: true
  })
  
  const citizens = await getTableRows({
    code: history,
    scope: history,
    table: 'citizens',
    json: true
  })
  
  assert({
    given: 'add resident entry',
    should: 'have resident row',
    actual: residents.rows[0].account,
    expected: firstuser
  })
  
  assert({
    given: 'add citizen entry',
    should: 'have resident row',
    actual: citizens.rows[0].account,
    expected: seconduser
  })

})

describe('org transaction entry', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const firstorg = 'testorg111'
  const secondorg = 'testorg222'

  const contracts = await initContracts({ settings, history, accounts, organization, token })
  
  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(firstorg, { authorization: `${history}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('org reset')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('create org, users')
  await contracts.accounts.adduser(firstuser, 'Bob', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Alice', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })

  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })
  
  console.log('create organization')
  const amount1 = '12.0000 SEEDS'
  const amount2 = '11.1111 SEEDS'

  await contracts.organization.create(firstuser, firstorg, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.organization.create(seconduser, secondorg, "Org Number 2", eosDevKey, { authorization: `${seconduser}@active` })

  console.log('add transaction entry from user to org')
  await contracts.history.trxentry(firstuser, firstorg, amount1, { authorization: `${history}@active` })
  
  const transactions = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'transactions',
    json: true
  })
  const orgtx = await getTableRows({
    code: history,
    scope: firstorg,
    table: 'orgtx',
    json: true
  })

  await contracts.history.trxentry(firstorg, secondorg, amount2, { authorization: `${history}@active` })

  const transactions2 = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'transactions',
    json: true
  })

  const orgtx2 = await getTableRows({
    code: history,
    scope: firstorg,
    table: 'orgtx',
    json: true
  })

  const orgtx3 = await getTableRows({
    code: history,
    scope: secondorg,
    table: 'orgtx',
    json: true
  })

  assert({
    given: 'transactions table',
    should: 'no change when org transact',
    actual: transactions2.rows.length,
    expected: transactions.rows.length
  })

  assert({
    given: 'org tx table',
    should: 'have 2 tx entry',
    actual: orgtx2.rows.length,
    expected: 2
  })

  delete transactions.rows[0].timestamp
  delete orgtx.rows[0].timestamp
  delete orgtx2.rows[0].timestamp
  delete orgtx2.rows[1].timestamp
  delete orgtx3.rows[0].timestamp

  assert({
    given: 'transactions table',
    should: 'have transaction entry',
    actual: transactions.rows[0],
    expected: {
      id: 0,
      to: firstorg,
      quantity: amount1,
    }
  })

  assert({
    given: 'org transactions table',
    should: 'have transaction entry',
    actual: orgtx.rows[0],
    expected: {
      id: 0,
      other: firstuser,
      quantity: amount1,
      in: 1
    }
  })

  assert({
    given: 'org transactions table 2',
    should: 'have transaction entries for both in and out',
    actual: orgtx2.rows,
    expected: [
      {
        "id": 0,
        "other": "seedsuseraaa",
        "in": 1,
        "quantity": "12.0000 SEEDS"
      },
      {
        "id": 1,
        "other": "testorg222",
        "in": 0,
        "quantity": "11.1111 SEEDS"
      }
    ]
  })
})

describe('Test migration', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const firstorg = 'testorg111'
  const secondorg = 'testorg222'

  const day = parseInt(new Date().setUTCHours(0,0,0,0) / 1000)
  const secondsPerDay = 60 * 60 * 24
  const numberOfDays = 4
  const numberOfTransactions = 3

  const contracts = await initContracts({ settings, history, accounts, organization, token })
  
  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.reset(firstorg, { authorization: `${history}@active` })
  await contracts.history.reset(secondorg, { authorization: `${history}@active` })
  await contracts.history.reset(history, { authorization: `${history}@active` })

  for (let currentDay = day; currentDay >= (day - (numberOfDays * secondsPerDay)); currentDay -= secondsPerDay) {
    console.log('reset history day:', currentDay)
    await contracts.history.deldailytrx(currentDay, { authorization: `${history}@active` })
  }
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  await contracts.settings.configure('batchsize', 20, { authorization: `${settings}@active` })

  console.log('org reset')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('create org, users')
  await contracts.accounts.adduser(firstuser, 'Bob', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Alice', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })

  const getTransactions = async (user, type = 'user') => {
    let trxs

    if (type === 'org') {
      trxs = await getTableRows({
        code: history,
        scope: user,
        table: 'orgtx',
        json: true
      })
    } else {
      trxs = await getTableRows({
        code: history,
        scope: user,
        table: 'transactions',
        json: true
      })
    }
    
    console.log(`${user}:`, trxs.rows)
    return trxs.rows
  }

  const getNewDailyTransactions = async (day, user) => {
    const date = new Date(day * 1000)
    
    const trxs = await getTableRows({
      code: history,
      scope: parseInt(date.setUTCHours(0, 0, 0, 0) / 1000),
      table: 'dailytrxs',
      json: true
    })

    const trxPoints = await getTableRows({
      code: history,
      scope: user,
      table: 'trxpoints',
      json: true
    })

    const qev = await getTableRows({
      code: history,
      scope: user,
      table: 'qevs',
      json: true
    })

    return {
      dailyTrxs: trxs.rows,
      trxPoints: trxPoints.rows,
      qevs: qev.rows
    }
  }

  const trxFirstuserBefore = await getTransactions(firstuser)
  const trxSeconduserBefore = await getTransactions(seconduser)
  const trxFirstorgBefore = await getTransactions(firstorg, 'org')
  const trxSecondorgBefore = await getTransactions(secondorg, 'org')

  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })
  
  console.log('create organization')
  await contracts.organization.create(firstuser, firstorg, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.organization.create(seconduser, secondorg, "Org Number 2", eosDevKey, { authorization: `${seconduser}@active` })

  console.log('add rep')
  await contracts.accounts.testsetrs(firstuser, 49, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(seconduser, 49, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(firstorg, 49, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(secondorg, 49, { authorization: `${accounts}@active` })

  console.log('add transactions')

  const now = parseInt(Date.now() / 1000)

  for (let currentDay = now; currentDay >= (now - (numberOfDays * secondsPerDay)); currentDay -= secondsPerDay) {
    console.log('current day = ', currentDay)
    for (let i = 1; i <= numberOfTransactions; i++) {
      await contracts.history.testentry(firstuser, firstorg, `${i}.0000 SEEDS`, currentDay, { authorization: `${history}@active` })
      await contracts.history.testentry(firstuser, seconduser, `${i}.0000 SEEDS`, currentDay, { authorization: `${history}@active` })
      await contracts.history.testentry(seconduser, firstorg, `${i}.0000 SEEDS`, currentDay, { authorization: `${history}@active` })
      await contracts.history.testentry(firstorg, secondorg, `${i}.0000 SEEDS`, currentDay, { authorization: `${history}@active` })
      await contracts.history.testentry(secondorg, seconduser, `${i}.0000 SEEDS`, currentDay, { authorization: `${history}@active` })
      await sleep(200)
    }
  }

  await contracts.history.migrateusers({ authorization: `${history}@active` })
  await sleep(45000)

  for (let currentDay = day, currentTimestamp = now; 
      currentDay >= (day - (numberOfDays * secondsPerDay)); 
      currentDay -= secondsPerDay, currentTimestamp -= secondsPerDay) {
    console.log('current day = ', currentDay)
    
    const infoFirstUser = await getNewDailyTransactions(currentDay, firstuser)
    const infoSecondUser = await getNewDailyTransactions(currentDay, seconduser)
    const infoFirstOrg = await getNewDailyTransactions(currentDay, firstorg)
    const infoSecondOrg = await getNewDailyTransactions(currentDay, secondorg)
    const infoHistory = await getNewDailyTransactions(currentDay, history)
    
    assert({
      given: 'current day migrated',
      should: 'have the correct information',
      actual: infoFirstUser.dailyTrxs,
      expected: [
        {
          id: 2,
          from: firstuser,
          to: firstorg,
          volume: 20000,
          qualifying_volume: 20000,
          from_points: 2,
          to_points: 2,
          timestamp: currentTimestamp
        },
        {
          id: 3,
          from: firstuser,
          to: seconduser,
          volume: 20000,
          qualifying_volume: 20000,
          from_points: 2,
          to_points: 2,
          timestamp: currentTimestamp
        },
        {
          id: 4,
          from: firstuser,
          to: firstorg,
          volume: 30000,
          qualifying_volume: 30000,
          from_points: 3,
          to_points: 3,
          timestamp: currentTimestamp
        },
        {
          id: 5,
          from: firstuser,
          to: seconduser,
          volume: 30000,
          qualifying_volume: 30000,
          from_points: 3,
          to_points: 3,
          timestamp: currentTimestamp
        },
        {
          id: 7,
          from: seconduser,
          to: firstorg,
          volume: 20000,
          qualifying_volume: 20000,
          from_points: 2,
          to_points: 2,
          timestamp: currentTimestamp
        },
        {
          id: 8,
          from: seconduser,
          to: firstorg,
          volume: 30000,
          qualifying_volume: 30000,
          from_points: 3,
          to_points: 3,
          timestamp: currentTimestamp
        },
        {
          id: 10,
          from: firstorg,
          to: secondorg,
          volume: 20000,
          qualifying_volume: 20000,
          from_points: 2,
          to_points: 2,
          timestamp: currentTimestamp
        },
        {
          id: 11,
          from: firstorg,
          to: secondorg,
          volume: 30000,
          qualifying_volume: 30000,
          from_points: 3,
          to_points: 3,
          timestamp: currentTimestamp
        },
        {
          id: 13,
          from: secondorg,
          to: seconduser,
          volume: 20000,
          qualifying_volume: 20000,
          from_points: 2,
          to_points: 2,
          timestamp: currentTimestamp
        },
        {
          id: 14,
          from: secondorg,
          to: seconduser,
          volume: 30000,
          qualifying_volume: 30000,
          from_points: 3,
          to_points: 3,
          timestamp: currentTimestamp
        }  
      ]
    })

    assert({
      given: 'current day migrated',
      should: 'have the correct info in transactions points',
      actual: [
        infoFirstUser.trxPoints.filter(trx => trx.timestamp === day),
        infoSecondUser.trxPoints.filter(trx => trx.timestamp === day),
        infoFirstOrg.trxPoints.filter(trx => trx.timestamp === day),
        infoSecondOrg.trxPoints.filter(trx => trx.timestamp === day)
      ],
      expected: [
        [{ timestamp: day, points: 10 }],
        [{ timestamp: day, points: 5 }],
        [{ timestamp: day, points: 15 }],
        [{ timestamp: day, points: 10 }]
      ]
    })

    assert({
      given: 'current day migrated',
      should: 'have the correct info in qev table',
      actual: [
        infoFirstUser.qevs.filter(trx => trx.timestamp === day),
        infoSecondUser.qevs.filter(trx => trx.timestamp === day),
        infoFirstOrg.qevs.filter(trx => trx.timestamp === day),
        infoSecondOrg.qevs.filter(trx => trx.timestamp === day),
        infoHistory.qevs.filter(trx => trx.timestamp === day)
      ],
      expected: [
        [{ timestamp: day, qualifying_volume: 100000 }],
        [{ timestamp: day, qualifying_volume: 50000 }],
        [{ timestamp: day, qualifying_volume: 50000 }],
        [{ timestamp: day, qualifying_volume: 50000 }],
        [{ timestamp: day, qualifying_volume: 250000 }]
      ]
    })

  }

  const totals = await getTableRows({
    code: history,
    scope: history,
    table: 'totals',
    json: true
  })

  assert({
    given: 'current day migrated',
    should: 'have the correct info in totals',
    actual: totals.rows, 
    expected: [
      {
        account: firstuser,
        total_volume: 600000,
        total_number_of_transactions: 30,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 15
      },
      {
        account: seconduser,
        total_volume: 300000,
        total_number_of_transactions: 15,
        total_incoming_from_rep_orgs: 15,
        total_outgoing_to_rep_orgs: 15
      },
      {
        account: firstorg,
        total_volume: 300000,
        total_number_of_transactions: 15,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 15
      },
      {
        account: secondorg,
        total_volume: 300000,
        total_number_of_transactions: 15,
        total_incoming_from_rep_orgs: 15,
        total_outgoing_to_rep_orgs: 0
      }
    ]
  })

}) 


