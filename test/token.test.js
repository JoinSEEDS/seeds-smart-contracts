const { describe } = require('riteway')

const { eos, names, getTableRows, getBalance, initContracts, isLocal } = require('../scripts/helper')
const { assert } = require('chai')

const { token, firstuser, seconduser, thirduser, history, accounts, harvest, settings } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

const getSupply = async () => {
  const { rows } = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'stat',
    json: true
  })

  return Number.parseInt(rows[0].supply)
}

describe('token.transfer.history', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ token, history, accounts, settings })
  
  const transfer = async () => await contracts.token.transfer(firstuser, seconduser, '10.0000 SEEDS', ``, { authorization: `${firstuser}@active` })
  const plant = async (user) => await contracts.token.transfer(user, "harvst.seeds", '0.0001 SEEDS', ``, { authorization: `${user}@active` })
  
  console.log('configure')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset history')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  console.log('plant seeds x')
  await contracts.token.transfer(firstuser, harvest, '0.0001 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '0.0001 SEEDS', '', { authorization: `${seconduser}@active` })

  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  
  console.log('transfer token')
  await transfer()
  await sleep(500)

  console.log('transfer 2')
  await transfer()
  await sleep(500)

  const stats = await getTableRows({
    code: token,
    scope: "SEEDS",
    table: 'trxstat',
    json: true
  })

  assert({
    given: 'transactions',
    should: 'have transaction stat entries',
    actual: stats.rows.filter( (item) => item.account == firstuser || item.account == seconduser),
    expected: [
      {
        "account": "seedsuseraaa",
        "transactions_volume": "20.0000 SEEDS",
        "total_transactions": 2,
        "incoming_transactions": 0,
        "outgoing_transactions": 2
      },
      {
        "account": "seedsuserbbb",
        "transactions_volume": "20.0000 SEEDS",
        "total_transactions": 2,
        "incoming_transactions": 2,
        "outgoing_transactions": 0
      }
    ]
  })


})

describe('token.transfer', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ token, harvest, settings, accounts })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('configure')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  console.log('plant seeds x')
  await contracts.token.transfer(firstuser, harvest, '0.0001 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '0.0001 SEEDS', '', { authorization: `${seconduser}@active` })

  let limit = 20
  const transfer = (n) => contracts.token.transfer(firstuser, seconduser, '10.0000 SEEDS', `x${n}`, { authorization: `${firstuser}@active` })

  const balances = [await getBalance(firstuser)]

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('plant SEEDS')
  await contracts.token.transfer(firstuser, harvest, '2.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log(`call transfer x${limit} times`)
  while (--limit >= 0) {
    await contracts.token.transfer(firstuser, seconduser, '1.0000 SEEDS', limit + "x", { authorization: `${firstuser}@active` })
  }
  const stats = await getTableRows({
    code: token,
    scope: "SEEDS",
    table: 'trxstat',
    json: true
  })

  console.log("trxstat "+JSON.stringify(stats, null, 2));

  let limitFailWithPlantedBalance = false
  try {
    await transfer()
    console.log('transferred over limit (NOT expected)')
  } catch (err) {
    limitFailWithPlantedBalance = true
    console.log('transfer over limit failed (as expected)')
  }

  balances.push(await getBalance(firstuser))

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })
  await contracts.token.transfer(firstuser, harvest, '0.0001 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '0.0001 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  limit = 7
  console.log(`call transfer x${limit} times`)
  while (--limit >= 0) {
    await transfer(limit)
  }

  let limitFailWithNoPlatedBalance = false
  try {
    await transfer()
    console.log('transferred over limit (NOT expected)')
  } catch (err) {
    limitFailWithNoPlatedBalance = true
    console.log('transfer over limit failed (as expected)')
  }

  assert({
    given: 'token.transfer called' + balances[0] + " -> " + balances[1],
    should: 'decrease user balance',
    actual: balances[1],
    expected: balances[0] - 20 - 2
  })

  assert({
    given: 'Limit of allowed transactions with planted seeds',
    should: 'fail when limit reached',
    actual: limitFailWithPlantedBalance,
    expected: true
  })

  assert({
    given: 'Limit of allowed transactions with no planted seeds',
    should: 'fail when limit reached',
    actual: limitFailWithNoPlatedBalance,
    expected: true
  })
})

describe('token.burn', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ token })

  const balances = []
  const supply = []

  balances.push(await getBalance(firstuser))
  supply.push(await getSupply())

  await contracts.token.burn(firstuser, '10.0000 SEEDS', { authorization: `${firstuser}@active` })

  balances.push(await getBalance(firstuser))
  supply.push(await getSupply())

  assert({
    given: 'token.burn called',
    should: 'decrease user balance',
    actual: balances[1],
    expected: balances[0] - 10
  })

  assert({
    given: 'token.burn called',
    should: 'decrease total supply',
    actual: supply[1],
    expected: supply[0] - 10
  })
})

describe('token calculate circulating supply', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ token })
  
  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  
  console.log('update circulating')
  await contracts.token.updatecirc({ authorization: `${token}@active` })
  
  console.log('transfer token')
  await contracts.token.transfer(firstuser, seconduser, '10.0000 SEEDS', `cc1`, { authorization: `${firstuser}@active` })
  
  const { rows } = await getTableRows({
    code: token,
    scope: token,
    table: 'circulating',
    json: true
  })
  
  console.log("circulating: "+JSON.stringify(rows, null, 2))

  assert({
    given: 'update circulating',
    should: 'have token circulating number',
    actual: rows.length,
    expected: 1
  })
})

describe('Test token.resetweekly', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ token, settings, accounts })
  
  console.log('configure')
  await contracts.settings.configure("batchsize", 2, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  await sleep(10 * 1000)

  console.log('update status')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('plant seeds x')
  await contracts.token.transfer(firstuser, harvest, '0.0001 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '0.0001 SEEDS', '', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, harvest, '0.0001 SEEDS', '', { authorization: `${thirduser}@active` })

  await contracts.token.transfer(firstuser, seconduser, '10.0000 SEEDS', ``, { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, thirduser, '10.0000 SEEDS', ``, { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, seconduser, '10.0000 SEEDS', ``, { authorization: `${thirduser}@active` })
  

  let balancesBeforeData = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'trxstat',
    json: true
  })

  //console.log("balancesBeforeData "+JSON.stringify(balancesBeforeData, null, 2))

  balancesBefore = balancesBeforeData.rows.filter(e => e.account == firstuser || e.account == seconduser || e.account == thirduser).map(row => row.outgoing_transactions)
 
  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  await sleep(10 * 1000)

  let balancesAfter = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'trxstat',
    json: true
  })

  balancesAfter = balancesAfter.rows.map(row => row.outgoing_transactions)

  await contracts.settings.reset({ authorization: `${settings}@active` })

  assert({
    given: 'called resetweekly',
    should: 'have trxstat table entry',
    actual: balancesBefore.splice(0, 3),
    expected: [1, 1, 1]
  })

  assert({
    given: 'called resetweekly',
    should: 'reset trxstat table',
    actual: balancesAfter.splice(0, 3),
    expected: [0, 0, 0]
  })

})

describe('transaction limits', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ token, settings, accounts, harvest })
  
  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })
  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('set tx limit min to 3')
  let minTrx = 3
  await contracts.settings.configure("txlimit.min", minTrx, { authorization: `${settings}@active` })

  console.log('set tx multiplier to 2')
  await contracts.settings.configure("txlimit.mul", 2, { authorization: `${settings}@active` })
  

  console.log('add users')
  await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, '', 'individual', { authorization: `${accounts}@active` })

  console.log('plant')
  await contracts.token.transfer(firstuser, harvest, '1.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '3.0000 SEEDS', '', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, harvest, '0.0001 SEEDS', '', { authorization: `${thirduser}@active` })

  let transfer = async (from, to) => {
    return await contracts.token.transfer(from, to, '0.0001 SEEDS', ``, { authorization: `${from}@active` })
  }

  let verifyLimit = async (from, to, limit) => {

    for(let i = 0; i < limit; i++) {
      await transfer(from, to)
    }

    let overLimit = false
    try {
      await transfer(from, to)
      overLimit = true
    } catch(err) {

    }
    assert({
      given: from + ' has limit of ' + limit,
      should: 'allow limit but not more',
      actual: overLimit,
      expected: false
    })
  }
  // first user can make min tx but not more
  console.log("first user has 1 planted, can make transactions" + minTrx)
  await verifyLimit(firstuser, seconduser, minTrx)

  console.log("second user has 3 planted, can make 6 transactions")
  await verifyLimit(seconduser, firstuser, 6)

  console.log("third user has nothing planted, can make minTrx transactions")
  await verifyLimit(thirduser, firstuser, minTrx)

})
