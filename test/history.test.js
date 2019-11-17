const { describe } = require("riteway")
const { eos, encodeName, getBalance, names, getTableRows } = require("../scripts/helper")
const { equals } = require("ramda")

const { harvest, firstuser, seconduser, history, accounts } = names

describe.only('make a transaction entry', async assert => {
  const historyContract = await eos.contract(history)
  const accountsContract = await eos.contract(accounts)
  
  console.log('history reset')
  await historyContract.reset(firstuser, { authorization: `${history}@active` })
  
  console.log('accounts reset')
  await accountsContract.reset({ authorization: `${accounts}@active` })
  
  console.log('update status')
  await accountsContract.adduser(firstuser, '', { authorization: `${accounts}@active` })
  await accountsContract.adduser(seconduser, '', { authorization: `${accounts}@active` })
  await accountsContract.testresident(firstuser, { authorization: `${accounts}@active` })
  await accountsContract.testcitizen(seconduser, { authorization: `${accounts}@active` })  

  console.log('add transaction entry')
  await historyContract.trxentry(firstuser, seconduser, '10.0000 SEEDS', '', { authorization: `${history}@active` })
  
  const { rows } = await getTableRows({
    code: history,
    scope: history,
    table: 'transactions',
    json: true
  })
  
  assert({
    given: 'transactions table',
    should: 'have transaction entry',
    actual: rows,
    expected: [{
      id: 0,
      from: firstuser,
      to: seconduser,
      quantity: '10.0000 SEEDS',
      fromstatus: 'resident',
      tostatus: 'citizen',
      memo: '',
    }]
  })
})

describe("make a history entry", async (assert) => {

    const history_contract = await eos.contract(history)

    console.log('history reset')
    await history_contract.reset(firstuser, { authorization: `${history}@active` })
    

    console.log('history make entry')
    await history_contract.historyentry(firstuser, "tracktest", 77,  "vasily", { authorization: `${history}@active` })

    console.log('check that history table has the entry')
    
    const { rows } = await getTableRows({
        code: history,
        scope: firstuser,
        table: "history",
        json: true
    })
    let timestamp = rows[0].timestamp

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
        actual: Math.round(timestamp/10),
        expected: Math.round(new Date()/10000),
    })


})