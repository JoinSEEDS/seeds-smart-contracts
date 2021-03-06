const { describe } = require("riteway")
const { names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")
const eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

const { firstuser, seconduser, thirduser, fourthuser, history, accounts, organization, token, settings, region } = names

function getBeginningOfDayInSeconds () {
  const now = new Date()
  return now.setUTCHours(0, 0, 0, 0) / 1000
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe('make a transaction entry', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ history, accounts, settings })

  const day = getBeginningOfDayInSeconds()

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })

  const firstuserRep = 10
  const seconduserRep = 49
  await contracts.accounts.testsetrs(firstuser, firstuserRep, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(seconduser, seconduserRep, { authorization: `${accounts}@active` })

  console.log('add transaction entry')
  await contracts.history.trxentry(firstuser, seconduser, '10.0000 SEEDS', { authorization: `${history}@active` })
  await sleep(2000)

  const { rows } = await getTableRows({
    code: history,
    scope: day,
    table: 'dailytrxs',
    json: true
  })

  const trxPointsTable = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'trxpoints',
    json: true
  })

  const trxPointsTable2 = await getTableRows({
    code: history,
    scope: seconduser,
    table: 'trxpoints',
    json: true
  })

  const qevVolumeTable = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'qevs',
    json: true
  })

  const qevVolumeTable2 = await getTableRows({
    code: history,
    scope: seconduser,
    table: 'qevs',
    json: true
  })

  const totals = await getTableRows({
    code: history,
    scope: history,
    table: 'totals',
    json: true
  })
  
  console.log("transactions result "+JSON.stringify(rows, null, 2))

  let txresult = rows[0]
  const timestamp = txresult.timestamp
  delete txresult.timestamp


  console.log('Delete ptrxs')
  await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

  await contracts.history.testptrx(0, { authorization: `${history}@active` })
  await contracts.history.testptrx(0, { authorization: `${history}@active` })

  await contracts.history.cleanptrxs({ authorization: `${history}@active` })
  await sleep(4000)

  const ptrxTable = await getTableRows({
    code: history,
    scope: history,
    table: 'ptrx',
    json: true
  })


  assert({
    given: 'transactions table',
    should: 'have transaction entry',
    actual: txresult,
    expected: {
      id: 0,
      from: firstuser,
      to: seconduser,
      volume: 10 * 10000,
      qualifying_volume: 10 * 10000,
      from_points: 10,
      to_points: 0
    }
  })

  assert({
    given: 'a transaction made by ' + firstuser,
    should: 'have transaction points entry',
    actual: trxPointsTable.rows,
    expected: [
      {
        timestamp: day,
        points: 10
      }
    ]
  })

  assert({
    given: 'a transaction received by ' + seconduser,
    should: 'not have transaction points entry',
    actual: trxPointsTable2.rows,
    expected: []
  })

  assert({
    given: 'a transaction made by ' + firstuser,
    should: 'have qualifying transactions entry',
    actual: qevVolumeTable.rows,
    expected: [
      {
        timestamp: day,
        qualifying_volume: 10 * 10000
      }
    ]
  })

  assert({
    given: 'a transaction received by ' + seconduser,
    should: 'not have qualifying volume entry',
    actual: qevVolumeTable2.rows,
    expected: []
  })

  assert({
    given: 'a transaction sent',
    should: 'have an entry in the totals table',
    actual: totals.rows,
    expected: [
      {
        account: firstuser,
        total_volume: 10 * 10000,
        total_number_of_transactions: 1,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 0
      }
    ]
  })

  assert({
    given: 'ptrx deleted',
    should: 'only delete the previous transactions',
    actual: ptrxTable.rows.length,
    expected: 1
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

describe('individual transactions', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ token, history, accounts, settings })

  const day = getBeginningOfDayInSeconds()

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.reset(thirduser, { authorization: `${history}@active` })
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(thirduser, { authorization: `${accounts}@active` })

  const firstuserRep = 1
  const seconduserRep = 49
  const thirduserRep = 99
  await contracts.accounts.testsetrs(firstuser, firstuserRep, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(seconduser, seconduserRep, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(thirduser, thirduserRep, { authorization: `${accounts}@active` })

  const transfer = async (from, to, quantity) => {
    await contracts.token.transfer(from, to, `${quantity}.0000 SEEDS`, 'test', { authorization: `${from}@active` })
  }

  const getTransactionEntries = async (user) => {
    const dailyTrx = await getTableRows({
      code: history,
      scope: day,
      table: 'dailytrxs',
      json: true
    })
  
    const trxPoints = await getTableRows({
      code: history,
      scope: user,
      table: 'trxpoints',
      json: true
    })
  
    const qevVolume = await getTableRows({
      code: history,
      scope: user,
      table: 'qevs',
      json: true
    })

    return {
      dailyTrx: dailyTrx.rows.map(r => {
        delete r.timestamp
        return r
      }),
      trxPoints: trxPoints.rows,
      qevVolume: qevVolume.rows
    }
  }

  console.log('add transaction entry')
  await transfer(firstuser, seconduser, 200) // trx points = 
  await transfer(firstuser, seconduser, 300)
  await transfer(firstuser, seconduser, 400)

  await sleep(2000)

  await contracts.accounts.testsetrs(seconduser, 1, { authorization: `${accounts}@active` })
  await transfer(firstuser, seconduser, 750)

  await transfer(seconduser, firstuser, 200)
  await transfer(seconduser, firstuser, 200)

  await transfer(seconduser, thirduser, 255)
  await transfer(seconduser, thirduser, 300)
  await transfer(seconduser, thirduser, 100)

  await transfer(thirduser, firstuser, 10)

  await sleep(4000)

  const infoFirstUser = await getTransactionEntries(firstuser)
  const infoSecondUser = await getTransactionEntries(seconduser)
  const infoThirdUser = await getTransactionEntries(thirduser)
  const historyTotal = await getTransactionEntries(history)

  const totals = await getTableRows({
    code: history,
    scope: history,
    table: 'totals',
    json: true
  })

  assert({
    given: 'transactions made',
    should: 'have daily transaction entries',
    actual: infoFirstUser.dailyTrx,
    expected: [
      {
        id: 2,
        from: firstuser,
        to: seconduser,
        volume: 4000000,
        qualifying_volume: 4000000,
        from_points: 396,
        to_points: 0
      },
      {
        id: 3,
        from: firstuser,
        to: seconduser,
        volume: 7500000,
        qualifying_volume: 7500000,
        from_points: 16,
        to_points: 0
      },
      {
        id: 4,
        from: seconduser,
        to: firstuser,
        volume: 2000000,
        qualifying_volume: 2000000,
        from_points: 5,
        to_points: 0
      },
      {
        id: 5,
        from: seconduser,
        to: firstuser,
        volume: 2000000,
        qualifying_volume: 2000000,
        from_points: 5,
        to_points: 0
      },
      {
        id: 6,
        from: seconduser,
        to: thirduser,
        volume: 2550000,
        qualifying_volume: 2550000,
        from_points: 510,
        to_points: 0
      },
      {
        id: 7,
        from: seconduser,
        to: thirduser,
        volume: 3000000,
        qualifying_volume: 3000000,
        from_points: 600,
        to_points: 0
      },
      {
        id: 9,
        from: thirduser,
        to: firstuser,
        volume: 100000,
        qualifying_volume: 100000,
        from_points: 1,
        to_points: 0
      }
    ]
  })

  assert({
    given: 'transaction made',
    should: 'have the correct entries in trx points tables',
    actual: [
      infoFirstUser.trxPoints,
      infoSecondUser.trxPoints, 
      infoThirdUser.trxPoints 
    ],
    expected: [
      [{ timestamp: day, points: 412 }],
      [{ timestamp: day, points: 1120 }],
      [{ timestamp: day, points: 1 }]
    ]
  })

  assert({
    given: 'transactions made',
    should: 'have the correct entries in qevs tables',
    actual: [ 
      infoFirstUser.qevVolume, 
      infoSecondUser.qevVolume, 
      infoThirdUser.qevVolume, 
      historyTotal.qevVolume 
    ],
    expected: [
      [{ timestamp: day, qualifying_volume: 11500000 }],
      [{ timestamp: day, qualifying_volume: 9550000 }],
      [{ timestamp: day, qualifying_volume: 100000 }],
      [{ timestamp: day, qualifying_volume: 21150000 }]
    ]
  })

  assert({
    given: 'transactions made',
    should: 'have the correct entries in totals table',
    actual: totals.rows,
    expected: [
      {
        account: firstuser,
        total_volume: 16500000,
        total_number_of_transactions: 4,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 0
      },
      {
        account: seconduser,
        total_volume: 10550000,
        total_number_of_transactions: 5,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 0
      },
      {
        account: thirduser,
        total_volume: 100000,
        total_number_of_transactions: 1,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 0
      }
    ]
  })

})


describe('org transaction entry', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const day = getBeginningOfDayInSeconds()

  const firstorg = 'testorg111'
  const secondorg = 'testorg222'

  const contracts = await initContracts({ settings, history, accounts, organization, token })
  
  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.reset(thirduser, { authorization: `${history}@active` })
  await contracts.history.reset(firstorg, { authorization: `${history}@active` })
  await contracts.history.reset(secondorg, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  
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
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(firstuser, 15, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(seconduser, 2, { authorization: `${accounts}@active` })

  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

  console.log('create organization')
  await contracts.organization.create(firstuser, firstorg, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.organization.create(seconduser, secondorg, "Org Number 2", eosDevKey, { authorization: `${seconduser}@active` })
  
  await contracts.accounts.testsetrs(firstorg, 60, { authorization: `${accounts}@active` })
  
  const transfer = async (from, to, quantity) => {
    await contracts.token.transfer(from, to, `${quantity}.0000 SEEDS`, 'test', { authorization: `${from}@active` })
  }

  const getTransactionEntries = async (user) => {
    const dailyTrx = await getTableRows({
      code: history,
      scope: day,
      table: 'dailytrxs',
      json: true
    })
  
    const trxPoints = await getTableRows({
      code: history,
      scope: user,
      table: 'trxpoints',
      json: true
    })
  
    const qevVolume = await getTableRows({
      code: history,
      scope: user,
      table: 'qevs',
      json: true
    })

    return {
      dailyTrx: dailyTrx.rows.map(r => {
        delete r.timestamp
        return r
      }),
      trxPoints: trxPoints.rows,
      qevVolume: qevVolume.rows
    }
  }

  console.log('add transaction entry from user to org')
  await transfer(firstuser, firstorg, 300)
  await transfer(firstuser, firstorg, 200)
  await transfer(firstuser, firstorg, 100)
  await transfer(firstuser, firstorg, 50)

  await transfer(firstorg, seconduser, 1)
  await transfer(firstorg, seconduser, 2)
  await transfer(firstorg, seconduser, 3)
  await transfer(firstorg, seconduser, 1)

  await transfer(firstorg, firstuser, 1)
  await transfer(firstorg, firstuser, 1)
  await transfer(firstorg, firstuser, 1)
  await transfer(firstorg, firstuser, 5)

  await transfer(firstorg, secondorg, 1)

  await transfer(seconduser, firstorg, 1)

  await sleep(5000)

  const infoFirstUser = await getTransactionEntries(firstuser)
  const infoFirstOrg = await getTransactionEntries(firstorg)
  const infoSecondUser = await getTransactionEntries(seconduser)
  const infoSecondOrg = await getTransactionEntries(secondorg)

  const totals = await getTableRows({
    code: history,
    scope: history,
    table: 'totals',
    json: true
  })

  assert({
    given: 'transactions made to orgs',
    should: 'have the correct daily transactions',
    actual: infoSecondUser.dailyTrx,
    expected: [
      {
        id: 0,
        from: firstuser,
        to: firstorg,
        volume: 3000000,
        qualifying_volume: 3000000,
        from_points: 364,
        to_points: 91
      },
      {
        id: 1,
        from: firstuser,
        to: firstorg,
        volume: 2000000,
        qualifying_volume: 2000000,
        from_points: 243,
        to_points: 61
      },
      {
        id: 5,
        from: firstorg,
        to: seconduser,
        volume: 20000,
        qualifying_volume: 20000,
        from_points: 1,
        to_points: 0
      },
      {
        id: 6,
        from: firstorg,
        to: seconduser,
        volume: 30000,
        qualifying_volume: 30000,
        from_points: 1,
        to_points: 0
      },
      {
        id: 10,
        from: firstorg,
        to: firstuser,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 1,
        to_points: 0
      },
      {
        id: 11,
        from: firstorg,
        to: firstuser,
        volume: 50000,
        qualifying_volume: 50000,
        from_points: 2,
        to_points: 0
      },
      {
        id: 12,
        from: firstorg,
        to: secondorg,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 0,
        to_points: 2
      },
      {
        id: 13,
        from: seconduser,
        to: firstorg,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 2,
        to_points: 1
      }
    ]
  })

  assert({
    given: 'transfer to org',
    should: 'have correct transaction points',
    actual: [ infoFirstUser.trxPoints, infoFirstOrg.trxPoints, infoSecondUser.trxPoints, infoSecondOrg.trxPoints ],
    expected: [
      [{ timestamp: day, points: 607 }],
      [{ timestamp: day, points: 158 }],
      [{ timestamp: day, points: 2 }],
      [{ timestamp: day, points: 2 }]
    ]
  })

  assert({
    given: 'transfer to org',
    should: 'have correct qualifying volume',
    actual: [ infoFirstUser.qevVolume, infoFirstOrg.qevVolume, infoSecondUser.qevVolume, infoSecondOrg.qevVolume ],
    expected: [
      [{ timestamp: day, qualifying_volume: 5000000 }],
      [{ timestamp: day, qualifying_volume: 120000 }],
      [{ timestamp: day, qualifying_volume: 10000 }],
      []
    ]
  })

  assert({
    given: 'transfer to org',
    should: 'have the correct entries in totals table',
    actual: totals.rows,
    expected: [
      {
        account: firstuser,
        total_volume: 6500000,
        total_number_of_transactions: 4,
        total_incoming_from_rep_orgs: 4,
        total_outgoing_to_rep_orgs: 4
      },
      {
        account: seconduser,
        total_volume: 10000,
        total_number_of_transactions: 1,
        total_incoming_from_rep_orgs: 4,
        total_outgoing_to_rep_orgs: 1
      },
      {
        account: firstorg,
        total_volume: 160000,
        total_number_of_transactions: 9,
        total_incoming_from_rep_orgs: 0,
        total_outgoing_to_rep_orgs: 1
      },
      {
        account: secondorg,
        total_volume: 0,
        total_number_of_transactions: 0,
        total_incoming_from_rep_orgs: 1,
        total_outgoing_to_rep_orgs: 0
      }
    ]
  })
})


describe('individual transactions', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const firstorg = 'firstorg'
  const secondorg = 'secondorg'

  const contracts = await initContracts({ token, history, accounts, settings, organization, region })

  const day = getBeginningOfDayInSeconds()

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.reset(thirduser, { authorization: `${history}@active` })
  await contracts.history.reset(firstorg, { authorization: `${history}@active` })
  await contracts.history.reset(secondorg, { authorization: `${history}@active` })
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset orgs')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('reset rgns')
  await contracts.region.reset({ authorization: `${region}@active` })

  const transfer = async (from, to, quantity) => {
    await contracts.token.transfer(from, to, `${quantity}.0000 SEEDS`, 'test', { authorization: `${from}@active` })
    await sleep(2000)
  }
  
  console.log('join users')
  const users = [firstuser, seconduser, thirduser]
  const rep = 49
  for (let i = 0; i < users.length; i++) {
    await contracts.accounts.adduser(users[i], '', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(users[i], rep, { authorization: `${accounts}@active` })
  }

  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

  console.log('create organization')
  await contracts.organization.create(firstuser, firstorg, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.organization.create(seconduser, secondorg, "Org Number 2", eosDevKey, { authorization: `${seconduser}@active` })
  await contracts.accounts.testsetrs(firstorg, 49, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(secondorg, 49, { authorization: `${accounts}@active` })
  await contracts.organization.teststatus(firstorg, 4, { authorization: `${organization}@active` })
    
  console.log('add regions')
  const keypair = await createKeypair();
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })
  const rgns = ['rgn1.rgn']
  for (let index = 0; index < rgns.length; index++) {
    const rgn = rgns[index]
    await contracts.token.transfer(users[index], region, "1.0000 SEEDS", "Initial supply", { authorization: `${users[index]}@active` })
    await contracts.region.create(
      users[index], 
      rgn, 
      'test rgn region',
      '{lat:0.0111,lon:1.3232}', 
      1.1, 
      1.23, 
      { authorization: `${users[index]}@active` })
  }

  await contracts.region.join(rgns[0], thirduser, { authorization: `${thirduser}@active` })

  await transfer(firstuser, thirduser, 1)
  await transfer(thirduser, seconduser, 1)
  await transfer(seconduser, firstorg, 1)
  await transfer(firstuser, secondorg, 1)
  await transfer(firstorg, secondorg, 1)

  const dailyTrx = await getTableRows({
    code: history,
    scope: day,
    table: 'dailytrxs',
    json: true
  })

  assert({
    given: 'transactions made',
    should: 'have the correct multipliers applied',
    actual: dailyTrx.rows.map(r => {
      delete r.timestamp
      return r
    }),
    expected: [
      { 
        id: 0,
        from: firstuser,
        to: thirduser,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 2,
        to_points: 0
      },
      { 
        id: 1,
        from: thirduser,
        to: seconduser,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 1,
        to_points: 0
      },
      { 
        id: 2,
        from: seconduser,
        to: firstorg,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 2,
        to_points: 1
      },
      {
        id: 3,
        from: firstuser,
        to: secondorg,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 1,
        to_points: 1
      },
      { 
        id: 4,
        from: firstorg,
        to: secondorg,
        volume: 10000,
        qualifying_volume: 10000,
        from_points: 1,
        to_points: 2
      }
    ]
  })

})


describe('Transaction CBS', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const firstorg = 'firstorg'
  const secondorg = 'secondorg'
  const thirdorg = 'thirdorg'
  const fourthorg = 'fourthorg'

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const orgs = [firstorg, secondorg, thirdorg, fourthorg]
  const orgsStauts = [4 /** thrivable */, 4, 3 /** regenerative */, 1 /** normal */]

  const contracts = await initContracts({ token, history, accounts, settings, organization, region })

  const day = getBeginningOfDayInSeconds()

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('history reset')
  await Promise.all(users.map(user => contracts.history.reset(user, { authorization: `${history}@active` })))
  await Promise.all(orgs.map(org => contracts.history.reset(org, { authorization: `${history}@active` })))
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset orgs')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('reset rgns')
  await contracts.region.reset({ authorization: `${region}@active` })

  const transfer = async (from, to, quantity) => {
    await contracts.token.transfer(from, to, `${quantity}.0000 SEEDS`, 'test', { authorization: `${from}@active` })
    await sleep(3000)
  }

  console.log('add users')
  await Promise.all(users.map(user => contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })))

  console.log('add orgs')
  for (let i = 0; i < orgs.length; i++) {
    const org = orgs[i]
    await contracts.token.transfer(firstuser, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.organization.create(firstuser, org, `${org} name`, eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.teststatus(org, orgsStauts[i], { authorization: `${organization}@active` })
  }
  
  console.log('add region')
  const keypair = await createKeypair()
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })
  const rgns = ['rgn1.rgn']
  for (let index = 0; index < rgns.length; index++) {
    const rgn = rgns[index]
    await contracts.token.transfer(users[index], region, "1.0000 SEEDS", "Initial supply", { authorization: `${users[index]}@active` })
    await contracts.region.create(
      users[index], 
      rgn, 
      'test rgn region',
      '{lat:0.0111,lon:1.3232}', 
      1.1, 
      1.23, 
      { authorization: `${users[index]}@active` })
  }

  await contracts.region.join(rgns[0], secondorg, { authorization: `${secondorg}@active` })

  console.log('make transactions')
  await transfer(seconduser, firstorg, 1)
  await transfer(firstuser, secondorg, 1)
  await transfer(thirduser, thirdorg, 1)
  await transfer(fourthuser, fourthorg, 1)
  await transfer(firstuser, secondorg, 1)

  const trxCbs = await getTableRows({
    code: history,
    scope: history,
    table: 'trxcbpreward',
    json: true
  })

  const cbsTable = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })

  await contracts.history.cleanptrxs({ authorization: `${history}@active` })

  assert({
    given: 'transactions made',
    should: 'have the correct trx cbp entries',
    actual: trxCbs.rows.map(r => {
      return {
        id: r.id,
        account: r.account,
        key: r.key
      }
    }),
    expected: [
      {
        id: 0,
        account: seconduser,
        key: 'buythriv.cbp'
      },
      {
        id: 1,
        account: firstuser,
        key: 'buythriv.cbp'
      },
      {
        id: 2,
        account: firstuser,
        key: 'buylocal.cbp'
      },
      {
        id: 3,
        account: thirduser,
        key: 'buyregen.cbp'
      }

    ]
  })

  assert({
    given: 'transactions made',
    should: 'have the correct cbps',
    actual: cbsTable.rows,
    expected: [
      { account: firstuser, community_building_score: 5, rank: 0 },
      { account: seconduser, community_building_score: 4, rank: 0 },
      { account: thirduser, community_building_score: 2, rank: 0 }
    ]
  })

})

