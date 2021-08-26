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
    console.log("balance for "+user+ " is "+balance[0])
  }
  return parseFloat(balance[0])
}

describe.only('star token exchange', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ startoken, token })
  
  await contracts.startoken.reset({ authorization: `${startoken}@active` })

  //await contracts.startoken.issue(startoken, '1000.0000 STARS', `init`, { authorization: `${startoken}@active` })
/////////
/////////


  supply = await getSupply()

  console.log('transfer seeds')
  await contracts.token.transfer(firstuser, startoken, '100.0000 SEEDS', ``, { authorization: `${firstuser}@active` })

  balance = await getStarsBalanceFloat(firstuser)

  console.log('transfer token')
  await contracts.startoken.transfer(firstuser, seconduser, '10.0000 STARS', ``, { authorization: `${firstuser}@active` })

  balance = await getStarsBalanceFloat(seconduser)

  console.log('transfer 2')

  const stats = await getTableRows({
    code: startoken,
    scope: "STARS",
    table: 'accounts',
    json: true
  })

  assert({
    given: 'transactions',
    should: 'have transaction stat entries',
    actual: stats.rows.filter( (item) => item.account == firstuser || item.account == seconduser),
    expected: [
    ]
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

