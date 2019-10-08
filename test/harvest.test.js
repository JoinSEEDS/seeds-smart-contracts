const { describe } = require("riteway")
const { eos, encodeName, getBalance, names, getTableRows } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, harvest, token, firstuser, seconduser, bank, settings } = names

describe("harvest", async assert => {
  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
  ]).then(([token, accounts, harvest, settings]) => ({
    token, accounts, harvest, settings
  }))

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('configure')
  await contracts.settings.configure("hrvstreward", 10000 * 100, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', { authorization: `${accounts}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('unplant seeds')
  await contracts.harvest.unplant(seconduser, '100.0000 SEEDS', { authorization: `${seconduser}@active` })

  const refundsAfterUnplanted = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })
  const balanceAfterUnplanted = await getBalance(seconduser)

  console.log('claim refund')
  await contracts.harvest.claimrefund(seconduser, 1, { authorization: `${seconduser}@active` })

  const refundsAfterClaimed = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })
  const balanceAfterClaimed = await getBalance(seconduser)

  console.log('cancel refund')
  await contracts.harvest.cancelrefund(seconduser, 1, { authorization: `${seconduser}@active` })

  const refundsAfterCanceled = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })

  console.log('sow seeds')
  await contracts.harvest.sow(seconduser, firstuser, '10.0000 SEEDS', { authorization: `${seconduser}@active` })

  console.log('calculate planted score')
  await contracts.harvest.calcplanted({ authorization: `${harvest}@active` })

  console.log('calculate transactions score')
  await contracts.harvest.calctrx({ authorization: `${harvest}@active` })

  console.log('calculate reputation multiplier')
  await contracts.harvest.calcrep({ authorization: `${harvest}@active` })

  const rewards = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'harvest',
    json: true
  })

  assert({
    given: 'unplant called',
    should: 'create refunds rows',
    actual: refundsAfterUnplanted.rows.length,
    expected: 12
  })

  assert({
    given: 'claimed refund',
    should: 'keep refunds rows',
    actual: refundsAfterClaimed.rows.length,
    expected: 12
  })

  assert({
    given: 'canceled refund',
    should: 'withdraw refunds to user',
    actual: refundsAfterCanceled.rows.length,
    expected: 0
  })

  assert({
    given: 'unplanting process',
    should: 'not change user balance before timeout',
    actual: balanceAfterUnplanted,
    expected: balanceAfterClaimed
  })

  assert({
    given: 'planted calculation',
    should: 'assign planted score to each user',
    actual: rewards.rows.map(row => ({
      account: row.account,
      score: row.planted_score
    })),
    expected: [{
      account: firstuser,
      score: 100
    }, {
      account: seconduser,
      score: 50
    }]
  })

  assert({
    given: 'transactions calculation',
    should: 'assign transactions score to each user',
    actual: rewards.rows.map(({ transactions_score }) => transactions_score),
    expected: [100, 50]
  })

  assert({
    given: 'reputation calculation',
    should: 'assign reputation multiplier to each user',
    actual: rewards.rows.map(({ reputation_score }) => reputation_score),
    expected: [50, 100]
  })

  assert({
    given: 'harvest process',
    should: 'distribute rewards based on contribution scores'
  })
})
