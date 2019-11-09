const { describe } = require('riteway')

const { eos, names, getTableRows, getBalance, initContracts } = require('../scripts/helper')

const { token, firstuser, seconduser } = names

const getSupply = async () => {
  const { rows } = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'stat',
    json: true
  })

  return Number.parseInt(rows[0].supply)
}

describe.only('token.transfer', async assert => {
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
