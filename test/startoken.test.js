const { describe } = require('riteway')

const { eos, names, getTableRows, getBalance, initContracts, isLocal } = require('../scripts/helper')
const { assert } = require('chai')

const { token, firstuser, seconduser, thirduser, startoken, accounts } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

const getSupply = async () => {
  const { rows } = await getTableRows({
    code: startoken,
    scope: 'STARS',
    table: 'stat',
    json: true
  })

  return Number.parseInt(rows[0].supply)
}

const getStarsBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.startoken, user, 'STARS')
  let num = parseFloat(balance[0])
  if (num == NaN) {
    console.log("nil balance for "+user+": "+JSON.stringify(balance))
  } else {
    console.log("STARS balance for "+user+ " is "+balance[0])
  }
  return parseFloat(balance[0])
}

const getSeedsBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  let num = parseFloat(balance[0])
  if (num == NaN) {
    console.log("nil balance for "+user+": "+JSON.stringify(balance))
  } else {
    console.log("SEEDS balance for "+user+ " is "+balance[0])
  }
  return parseFloat(balance[0])
}

describe.only('star token exchange', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ startoken, token })
  
  // await contracts.startoken.reset({ authorization: `${startoken}@active` })

  //await contracts.startoken.issue(startoken, '1000.0000 STARS', `init`, { authorization: `${startoken}@active` })
/////////
/////////


  supply = await getSupply()

  console.log('transfer seeds')

  const starsBalance0 = await getStarsBalanceFloat(firstuser)
  const seedsBalance0 = await getSeedsBalanceFloat(firstuser)

  await contracts.token.transfer(firstuser, startoken, '100.0000 SEEDS', ``, { authorization: `${firstuser}@active` })

  const seedsBalance1 = await getSeedsBalanceFloat(firstuser)
  const starsBalance1 = await getStarsBalanceFloat(firstuser)
  const user2stars1 = await getStarsBalanceFloat(seconduser)

  assert({
    given: 'exchange seeds to stars',
    should: 'get stars for seeds',
    actual: starsBalance1 - starsBalance0,
    expected: 8.0137
  })

  assert({
    given: 'send seeds to stars',
    should: 'have less seeds',
    actual: seedsBalance1 - seedsBalance0,
    expected: -100.0
  })

  console.log('transfer token')
  await contracts.startoken.transfer(firstuser, seconduser, '8.0000 STARS', ``, { authorization: `${firstuser}@active` })

  const user2stars2 = await getStarsBalanceFloat(seconduser)
  const user2seeds2 = await getSeedsBalanceFloat(seconduser)
  const seedsBalance2 = await getSeedsBalanceFloat(firstuser)
  const starsBalance2 = await getStarsBalanceFloat(firstuser)

  assert({
    given: 'transfer token',
    should: 'have sent token',
    actual: user2stars2 - user2stars1,
    expected: 8.0
  })

  assert({
    given: 'transfer token',
    should: 'have sent token',
    actual: starsBalance2 - starsBalance1,
    expected: -8.0
  })

  console.log('transfer 2')

  console.log('transfer token')
  await contracts.startoken.transfer(seconduser, startoken, '8.0137 STARS', ``, { authorization: `${seconduser}@active` })

  const user2stars3 = await getStarsBalanceFloat(seconduser)
  const user2seeds3 = await getSeedsBalanceFloat(seconduser)

  assert({
    given: 'exchange stars for seeds',
    should: 'have sent token',
    actual: user2stars3 - user2stars2,
    expected: -8.0137
  })

  assert({
    given: 'exchange stars for seeds',
    should: 'get seeds',
    actual: user2seeds3 - user2seeds2 - 100.0 < 0.0000001,
    expected: true
  })

  const stats = await getTableRows({
    code: startoken,
    scope: "STARS",
    table: 'accounts',
    json: true
  })


})

describe('token.transfer', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ startoken })


  let limit = 20
  const transfer = (n) => contracts.startoken.transfer(firstuser, seconduser, '1.0000 STARS', `x${n}`, { authorization: `${firstuser}@active` })

  const balances = [await getBalance(firstuser)]


    await contracts.token.transfer(firstuser, seconduser, '1.0000 STARS', limit + "x", { authorization: `${firstuser}@active` })
  
  assert({
    given: 'token.transfer called',
    should: 'decrease user balance',
    actual: balances[1],
    expected: balances[0] - 202
  })

})

