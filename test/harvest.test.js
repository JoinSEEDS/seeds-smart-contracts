const { describe } = require("riteway")
const { eos, encodeName } = require("./helper")
const { equals } = require("ramda")

const getBalance = async (user) => {
  const balance = await eos.getCurrencyBalance('token', user, 'SEEDS')
  return Number.parseInt(balance[0])
}

describe("harvest", async assert => {
  const harvest = await eos.contract('harvest')
  const token = await eos.contract('token')

  const firstUserInitialBalance = await getBalance('user1')
  const secondUserInitialBalance = await getBalance('user2')
  
  // reset
  await harvest.reset({ authorization: 'harvest@active' })

  // configure
  await harvest.configure('periodreward', 2 * 10000, { authorization: 'harvest@active' })
  await harvest.configure('periodblocks', 1, { authorization: 'harvest@active' })
  await harvest.configure('tokenaccnt', encodeName('token', false), { authorization: 'harvest@active' })
  await harvest.configure('bankaccnt', encodeName('owner', false), { authorization: 'harvest@active' })
  
  // plant
  await token.transfer('user1', 'harvest', '100.0000 SEEDS', '', { authorization: 'user1@active' })
  await token.transfer('user2', 'harvest', '200.0000 SEEDS', '', { authorization: 'user2@active' })
  
  // unplant
  await harvest.unplant('user2', '100.0000 SEEDS', { authorization: 'user2@active' })
  
  // harvest
  await harvest.onperiod({ authorization: 'harvest@active' })
  
  // withdraw
  await harvest.claimreward('user1', { authorization: 'user1@active' })
  await harvest.claimreward('user2', { authorization: 'user2@active' })
  
  const firstUserFinalBalance = await getBalance('user1')
  const secondUserFinalBalance = await getBalance('user2')

  const firstUserReward = firstUserFinalBalance - firstUserInitialBalance
  const secondUserReward = secondUserFinalBalance - secondUserInitialBalance

  assert({
    given: 'equal amount of planted seeds',
    should: 'distribute rewards equally',
    actual: equals(firstUserReward, secondUserReward),
    expected: true
  })
})
