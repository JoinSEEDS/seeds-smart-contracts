const { describe } = require("riteway")
const { eos, encodeName, getBalance, names, getTableRows, isLocal } = require("../scripts/helper")
const { equals } = require("ramda")

const { harvest, firstuser, seconduser, history, accounts, settings } = names


function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe('make a transaction entry', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const historyContract = await eos.contract(history)
  const accountsContract = await eos.contract(accounts)
  const settingsContract = await eos.contract(settings)
  
  console.log('history reset')
  await historyContract.reset(firstuser, { authorization: `${history}@active` })
  
  console.log('accounts reset')
  await accountsContract.reset({ authorization: `${accounts}@active` })

  console.log('settings reset')
  await settingsContract.reset({ authorization: `${settings}@active` })
  
  console.log('update status')
  await accountsContract.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await accountsContract.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await accountsContract.testresident(firstuser, { authorization: `${accounts}@active` })
  await accountsContract.testcitizen(seconduser, { authorization: `${accounts}@active` })  

  console.log('add transaction entry')
  await historyContract.trxentry(firstuser, seconduser, '10.0000 SEEDS', { authorization: `${history}@active` })
  
  const { rows } = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'transactions',
    json: true
  })

  console.log("clear history - wait 1 second");
  await sleep(1000)

  console.log("make moon cycle short for testing");
  await settingsContract.configure("mooncyclesec", 0, { authorization: `${settings}@active` })
  await historyContract.trxentry(firstuser, seconduser, '20.0000 SEEDS', { authorization: `${history}@active` })

  await historyContract.clearoldtrx(firstuser, { authorization: `${history}@active` })


  const txAfterClear = await getTableRows({
    code: history,
    scope: firstuser,
    table: 'transactions',
    json: true
  })
  console.log("txAfterClear "+JSON.stringify(txAfterClear))
  
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

  assert({
    given: 'tx cleared',
    should: 'have no transaction entry',
    actual: txAfterClear.rows.length,
    expected: 1
  })

})

describe("make a history entry", async (assert) => {

    const history_contract = await eos.contract(history)

    console.log('history reset')
    await history_contract.reset(firstuser, { authorization: `${history}@active` })
    
    console.log('history make entry')
    await history_contract.historyentry(firstuser, "tracktest", 77,  "vasily", { authorization: `${history}@active` })
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
  await history_contract.addresident(firstuser, { authorization: `${history}@active` })
  
  console.log('add citizen')
  await history_contract.addcitizen(seconduser, { authorization: `${history}@active` })

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