const { describe } = require('riteway')

const { eos, names, getTableRows, getBalance, initContracts, isLocal } = require('../scripts/helper')

const { token, firstuser, seconduser, history, accounts } = names

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

  const contract = await eos.contract(token)
  const historyContract = await eos.contract(history)
  const accountsContract = await eos.contract(accounts)
  
  const transfer = () => contract.transfer(firstuser, seconduser, '10.0000 SEEDS', ``, { authorization: `${firstuser}@active` })
  
  console.log('reset history')
  await historyContract.reset(firstuser, { authorization: `${history}@active` })

  console.log('accounts reset')
  await accountsContract.reset({ authorization: `${accounts}@active` })

  console.log('update status')
  await accountsContract.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
  await accountsContract.adduser(seconduser, '', 'individual', { authorization: `${accounts}@active` })
  await accountsContract.testresident(firstuser, { authorization: `${accounts}@active` })
  await accountsContract.testcitizen(seconduser, { authorization: `${accounts}@active` })
  
  console.log('transfer token')
  await transfer()
  
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

describe('token.transfer', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(token)

  let limit = 10;
  const transfer = (n) => contract.transfer(firstuser, seconduser, '10.0000 SEEDS', `x${n}`, { authorization: `${firstuser}@active` })

  const balances = [await getBalance(firstuser)]
  
  console.log('reset token stats')
  await contract.resetweekly({ authorization: `${token}@active` })

  console.log(`call transfer x${limit} times`)
  while (--limit >= 0) {
    await transfer(limit)
  }

  try {
    await transfer()
    console.log('transferred over limit (NOT expected)')
  } catch (err) {
    console.log('transfer over limit failed (as expected)')
  }

  balances.push(await getBalance(firstuser))

  assert({
    given: 'token.transfer called',
    should: 'decrease user balance',
    actual: balances[1],
    expected: balances[0] - 100
  })
})

describe('token.burn', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(token)

  const balances = []
  const supply = []

  balances.push(await getBalance(firstuser))
  supply.push(await getSupply())

  await contract.burn(firstuser, '10.0000 SEEDS', { authorization: `${firstuser}@active` })

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

  const contract = await eos.contract(token)
  
  const transfer = () => contract.transfer(firstuser, seconduser, '10.0000 SEEDS', ``, { authorization: `${firstuser}@active` })
  
  console.log('update circulating')
  await contract.updatecirc({ authorization: `${token}@active` })
  
  console.log('transfer token')
  await transfer()
  
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
