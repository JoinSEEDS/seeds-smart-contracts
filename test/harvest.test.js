const { describe } = require("riteway")
const { eos, encodeName, getBalance, names } = require("../scripts/helper")
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
  await contracts.accounts.adduser(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, { authorization: `${accounts}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '140.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '210.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('unplant seeds')
  await contracts.harvest.unplant(seconduser, '100.0000 SEEDS', { authorization: `${seconduser}@active` })

  console.log('sow seeds')
  await contracts.harvest.sow(seconduser, firstuser, '10.0000 SEEDS', { authorization: `${seconduser}@active` })

  console.log('distribute seeds')
  await contracts.harvest.onperiod({ authorization: `${harvest}@active` })

  assert({
    given: 'harvest',
    should: 'distribute rewards',
    actual: await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table: 'balances',
      json: true
    }),
    expected: {
      rows: [
        { account: 'firstuser', planted: '150.0000 SEEDS', reward: '66.6666 SEEDS' },
        { account: 'harvest', planted: '250.0000 SEEDS', reward: '99.9999 SEEDS' },
        { account: 'seconduser', planted: '100.0000 SEEDS', reward: '33.3333 SEEDS' }
      ],
      more: false
    }
  })

  console.log('claim rewards')
  await contracts.harvest.claimreward(firstuser, '66.6666 SEEDS', { authorization: `${firstuser}@active` })
  await contracts.harvest.claimreward(seconduser, '33.3333 SEEDS', { authorization: `${seconduser}@active` })

  assert({
    given: 'harvest',
    should: 'distribute rewards',
    actual: await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table: 'balances',
      json: true
    }),
    expected: {
      rows: [
        { account: 'firstuser', planted: '150.0000 SEEDS', reward: '0.0000 SEEDS' },
        { account: 'harvest', planted: '250.0000 SEEDS', reward: '0.0000 SEEDS' },
        { account: 'seconduser', planted: '100.0000 SEEDS', reward: '0.0000 SEEDS' }
      ],
      more: false
    }
  })
})
