const { describe } = require("riteway")
const { eos, encodeName, getBalance, names } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, harvest, token, firstuser, seconduser, bank } = names

describe("harvest", async assert => {
  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
  ]).then(([token, accounts, harvest]) => ({
    token, accounts, harvest
  }))

  const firstUserInitialBalance = await getBalance(firstuser)
  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, { authorization: `${accounts}@active` })

  console.log('harvest configure')
  await contracts.harvest.configure('periodreward', 2 * 10000, { authorization: `${harvest}@active` })
  await contracts.harvest.configure('periodblocks', 1, { authorization: `${harvest}@active` })
  await contracts.harvest.configure('tokenaccnt', encodeName(token, false), { authorization: `${harvest}@active` })
  await contracts.harvest.configure('bankaccnt', encodeName(bank, false), { authorization: `${harvest}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('unplant seeds')
  await contracts.harvest.unplant(seconduser, '100.0000 SEEDS', { authorization: `${seconduser}@active` })

  console.log('distribute seeds')
  await contracts.harvest.onperiod({ authorization: `${harvest}@active` })

  console.log('claim rewards')
  await contracts.harvest.claimreward(firstuser, { authorization: `${firstuser}@active` })
  await contracts.harvest.claimreward(seconduser, { authorization: `${seconduser}@active` })

  const firstUserFinalBalance = await getBalance(firstuser)
  const secondUserFinalBalance = await getBalance(seconduser)

  const firstUserReward = firstUserFinalBalance - firstUserInitialBalance
  const secondUserReward = secondUserFinalBalance - secondUserInitialBalance

  const config = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'config',
    json: true,
  })
  
  const harvestTimestampInSeconds = config.rows.find(item => item.param == 'lastharvest').value

  assert({
    given: 'equal amount of planted seeds',
    should: 'distribute rewards equally',
    actual: equals(firstUserReward, secondUserReward),
    expected: true
  })
  
  assert({
    given: 'last harvest',
    should: 'save block timestamp',
    actual: harvestTimestampInSeconds > 0 && harvestTimestampInSeconds < (new Date / 1000),
    expected: true
  })
})
