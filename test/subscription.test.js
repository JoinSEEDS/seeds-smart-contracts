const { describe } = require("riteway")
const { eos, encodeName, getBalance, names } = require('../scripts/helper')

const { accounts, subscription, token, application } = names

describe("subscription", async assert => {
  const contracts = Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(subscription),
  ]).then(([token, accounts, subscription]) => ({
    token, accounts, subscription
  }))

  const applicationInitialBalance = await getBalance(application)

  await contracts.subscription.reset({ authorization: `${subscription}@active` })

  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  await contracts.accounts.addapp(application, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, { authorization: `${accounts}@active` })

  await contracts.subscription.create(application, '10.0000 SEEDS', { authorization: `${application}@active` })

  await contracts.subscription.update(application, 1, '20.0000 SEEDS', { authorization: `${application}@active` })

  await contracts.token.transfer(firstuser, subscription, '10.0000 SEEDS', application, { authorization: `${firstuser}@active` })

  await contracts.subscription.disable(firstuser, application)

  await contracts.subscription.enable(firstuser, application)

  await contracts.subscription.claimpayout(application)

  const applicationFinalBalance = await getBalance(application)

  assert({
    given: 'subscribed user',
    should: 'receive payout',
    actual: applicationFinalBalance > applicationInitialBalance,
    expected: true
  })
})
