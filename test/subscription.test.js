const { describe } = require("riteway")
const { eos, encodeName, getBalance, names } = require('../scripts/helper')

const { accounts, subscription, token, application, firstuser, seconduser } = names

describe("subscription", async assert => {
  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(subscription),
  ]).then(([token, accounts, subscription]) => ({
    token, accounts, subscription
  }))

  const applicationInitialBalance = await getBalance(application)

  console.log('subscription reset')
  await contracts.subscription.reset({ authorization: `${subscription}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('add application')
  await contracts.accounts.addapp(application, { authorization: `${accounts}@active` })

  const apps = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'apps',
    json: true,
  })

  console.log("APPS ", JSON.stringify(apps))

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'nickname1', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'secondnick', { authorization: `${accounts}@active` })

  console.log('create provider')
  await contracts.subscription.create(application, '10.0000 SEEDS', { authorization: `${application}@active` })

  console.log('update provider')
  await contracts.subscription.update(application, 1, '20.0000 SEEDS', { authorization: `${application}@active` })

  console.log('increase subscription')
  await contracts.token.transfer(firstuser, subscription, '10.0000 SEEDS', application, { authorization: `${firstuser}@active` })

  const subs = await eos.getTableRows({
    code: subscription,
    scope: application,
    table: 'subs',
    json: true,
  })
  console.log("subs ", JSON.stringify(subs))


  console.log('enable subscription')
  await contracts.subscription.enable(firstuser, application, { authorization: `${firstuser}@active` })

  console.log('disable subscription')
  await contracts.subscription.disable(firstuser, application, { authorization: `${firstuser}@active` })


  console.log('every period')
  await contracts.subscription.onblock({ authorization: `${subscription}@active` })

  console.log('claim payout')
  await contracts.subscription.claimpayout(application)

  const applicationFinalBalance = await getBalance(application)

  assert({
    given: 'subscribed user',
    should: 'receive payout',
    actual: applicationFinalBalance > applicationInitialBalance,
    expected: true
  })
})
