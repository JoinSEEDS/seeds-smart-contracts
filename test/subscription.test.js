const { describe } = require("riteway")
const { eos, encodeName } = require("./helper")

const getBalance = async (user) => {
  const balance = await eos.getCurrencyBalance('token', user, 'SEEDS')
  return Number.parseInt(balance[0])
}

describe("subscription", async assert => {
  const accounts = await eos.contract('accounts')
  const subscription = await eos.contract('subscription')
  const token = await eos.contract('token')

  const applicationInitialBalance = await getBalance('application')

  await subscription.reset({ authorization: 'subscription@active' })

  await accounts.addapp('application', { authorization: 'accounts@active' })
  
  await accounts.adduser('user1', { authorization: 'accounts@active' })
  
  await subscription.create('application', '10.0000 SEEDS', { authorization: 'application@active' })
  
  await subscription.update('application', 1, '20.0000 SEEDS', { authorization: 'application@active' })
  
  await token.transfer('user1', 'subscription', '10.0000 SEEDS', 'application', { authorization: 'user1@active' })

  await subscription.disable('user1', 'application')
  
  await subscription.enable('user1', 'application')

  await subscription.claimpayout('application')

  const applicationFinalBalance = await getBalance('application')

  assert({
    given: 'subscribed user',
    should: 'receive payout',
    actual: applicationFinalBalance > applicationInitialBalance,
    expected: true
  })
})
