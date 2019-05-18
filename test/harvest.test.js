const { describe } = require("riteway")
const { eos, encodeName, getBalance, names } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, harvest, token, firstuser, seconduser, owner } = names

describe("harvest", async assert => {
  const contracts = Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
  ]).then(([token, accounts, harvest]) => ({
    token, accounts, harvest
  }))

  const firstUserInitialBalance = await getBalance(firstuser)
  const secondUserInitialBalance = await getBalance(seconduser)

  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  await contracts.accounts.adduser(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, { authorization: `${accounts}@active` })

  await contracts.harvest.configure('periodreward', 2 * 10000, { authorization: `${harvest}@active` })
  await contracts.harvest.configure('periodblocks', 1, { authorization: `${harvest}@active` })
  await contracts.harvest.configure('tokenaccnt', encodeName(token, false), { authorization: `${harvest}@active` })
  await contracts.harvest.configure('bankaccnt', encodeName(owner, false), { authorization: `${harvest}@active` })

  await contracts.token.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${user1}@active` })
  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${user2}@active` })

  await contracts.harvest.unplant(seconduser, '100.0000 SEEDS', { authorization: `${user2}@active` })

  await contracts.harvest.onperiod({ authorization: `${harvest}@active` })

  await contracts.harvest.claimreward(firstuser, { authorization: `${user1}@active` })
  await contracts.harvest.claimreward(seconduser, { authorization: `${user2}@active` })

  const firstUserFinalBalance = await getBalance(firstuser)
  const secondUserFinalBalance = await getBalance(seconduser)

  const firstUserReward = firstUserFinalBalance - firstUserInitialBalance
  const secondUserReward = secondUserFinalBalance - secondUserInitialBalance

  assert({
    given: 'equal amount of planted seeds',
    should: 'distribute rewards equally',
    actual: equals(firstUserReward, secondUserReward),
    expected: true
  })
})
