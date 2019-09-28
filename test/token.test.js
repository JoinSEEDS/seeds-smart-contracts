const { describe } = require('riteway')

const { eos, names, getTableRows, getBalance, initContracts } = require('../scripts/helper')

const { token, firstuser, seconduser } = names

describe('Token', async assert => {
  const contracts = await initContracts({ token })

  const balanceBefore = await getBalance(firstuser)

  const statsBefore = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'stat',
    json: true
  })

  const supplyBefore = Number.parseFloat(statsBefore.rows[0].supply)

  await contracts.token.burn(firstuser, '1000.0000 SEEDS', { authorization: `${firstuser}@active` })

  const balanceAfter = await getBalance(firstuser)

  const statsAfter = await getTableRows({
    code: token,
    scope: 'SEEDS',
    table: 'stat',
    json: true
  })

  const supplyAfter = Number.parseFloat(statsAfter.rows[0].supply)

  assert({
    given: 'burned tokens',
    should: 'change user balance',
    actual: balanceBefore - balanceAfter,
    expected: 1000
  })

  assert({
    given: 'burned tokens',
    should: 'change total supply',
    actual: supplyBefore - supplyAfter,
    expected: 1000
  })
})
