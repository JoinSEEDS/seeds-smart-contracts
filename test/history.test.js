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

describe('Migration test', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const firstorg = 'testorg111'
  const secondorg = 'testorg222'

  const contracts = await initContracts({ settings, history, accounts, organization, token })
  
  console.log('history reset')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })
  await contracts.history.reset(seconduser, { authorization: `${history}@active` })
  await contracts.history.reset(firstorg, { authorization: `${history}@active` })
  await contracts.history.reset(secondorg, { authorization: `${history}@active` })

  await contracts.history.resetmigrate(firstuser, { authorization: `${history}@active` })
  await contracts.history.resetmigrate(seconduser, { authorization: `${history}@active` })
  await contracts.history.resetmigrate(firstorg, { authorization: `${history}@active` })
  await contracts.history.resetmigrate(secondorg, { authorization: `${history}@active` })
  
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
  await contracts.history.testtrx(firstuser, firstorg, amount1, { authorization: `${history}@active` })
  await contracts.history.testtrx(seconduser, secondorg, amount1, { authorization: `${history}@active` })
  await contracts.history.testtrx(firstorg, seconduser, amount2, { authorization: `${history}@active` })
  await contracts.history.testtrx(secondorg, seconduser, amount2, { authorization: `${history}@active` })

  const getTransactions = async () => {

    const transactions1 = await getTableRows({
      code: history,
      scope: firstuser,
      table: 'transactions',
      json: true
    })
  
    const transactions2 = await getTableRows({
      code: history,
      scope: seconduser,
      table: 'transactions',
      json: true
    })
  
    const transactions3 = await getTableRows({
      code: history,
      scope: firstorg,
      table: 'orgtx',
      json: true
    })
  
    const transactions4 = await getTableRows({
      code: history,
      scope: secondorg,
      table: 'orgtx',
      json: true
    })

    return [
      transactions1.rows,
      transactions2.rows,
      transactions3.rows,
      transactions4.rows
    ]

  }

  const getTransactionsMigration = async () => {

    const transactions1 = await getTableRows({
      code: history,
      scope: firstuser,
      table: 'transactionm',
      json: true
    })
  
    const transactions2 = await getTableRows({
      code: history,
      scope: seconduser,
      table: 'transactionm',
      json: true
    })
  
    const transactions3 = await getTableRows({
      code: history,
      scope: firstorg,
      table: 'orgtxm',
      json: true
    })
  
    const transactions4 = await getTableRows({
      code: history,
      scope: secondorg,
      table: 'orgtxm',
      json: true
    })

    return [
      transactions1.rows,
      transactions2.rows,
      transactions3.rows,
      transactions4.rows
    ]

  }

  const transactionsBefore = await getTransactions()
  const transactionsMigrationBefore = await getTransactionsMigration()

  console.log(transactionsMigrationBefore)
  console.log(transactionsBefore)

  console.log('migrate back')

  await contracts.history.migratebacks({ authorization: `${history}@active` })
  await sleep(300)

  const transactionsAfter = await getTransactions()
  const transactionsMigrationAfter = await getTransactionsMigration()

  assert({
    given: 'before migration',
    should: 'be empty',
    expected: [[], [], [], []],
    actual: transactionsBefore
  })

  assert({
    given: 'migration finished',
    should: 'have the same information',
    expected: transactionsMigrationBefore,
    actual: transactionsAfter
  })

  // assert({
  //   given: 'migration finished',
  //   should: 'be empty',
  //   expected: [[], [], [], []],
  //   actual: transactionsMigrationAfter
  // })

})

