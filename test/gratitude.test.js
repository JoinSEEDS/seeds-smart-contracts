const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, initContracts, getBalance } = require("../scripts/helper")
const eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

const { firstuser, seconduser, settings, gratitude, accounts, token, bank } = names

describe('gratitude general', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ gratitude, accounts, settings, token })

  const get_settings = async (key) => {
    const value = await eos.getTableRows({
      code: settings,
      scope: settings,
      table: 'config',
      lower_bound: key,
      upper_bound: key,
      json: true,
    })
    return value.rows[0].value
  }

  const getRemainingGratitude = async account => {
    const balanceTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'balances',
      json: true
    })
    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].remaining)
  }

  const checkRemainingGratitude = async (account, expected) => {
    const remaining = await getRemainingGratitude(account)
    assert({
      given: `${account} given gratitude`,
      should: 'have the correct remaining balance',
      actual: remaining,
      expected: expected
    })
  }

  const getReceivedGratitude = async account => {
    const balanceTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'balances',
      json: true
    })
    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].received)
  }

  const checkReceivedGratitude = async (account, expected) => {
    const received = await getReceivedGratitude(account)
    assert({
      given: `${account} received gratitude`,
      should: 'have the correct received balance',
      actual: received,
      expected: expected
    })
  }

  console.log('gratitude reset')
  await contracts.gratitude.reset({ authorization: `${gratitude}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  const initialGratitude = await get_settings("gratz.gen") / 10000; // in GRATZ

  console.log('add SEEDS users')
  const users = [firstuser, seconduser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
  }

  const amount = 1000
  const contractBalanceBefore = await getBalance(gratitude) || 0
  console.log('refill gratitude contract')
  await contracts.token.transfer(firstuser, gratitude, `${amount}.0000 SEEDS`, 'test', { authorization: `${firstuser}@active` })
  const contractBalanceStart = await getBalance(gratitude)

  console.log('give gratitude')
  const transferAmount = 10
  await contracts.gratitude.give(firstuser, seconduser, `${transferAmount}.0000 GRATZ`, '', { authorization: `${firstuser}@active` })

  checkRemainingGratitude(firstuser, initialGratitude-transferAmount)
  checkReceivedGratitude(seconduser, transferAmount)

  console.log('restart gratitude round')

  const secondBalanceBefore = await getBalance(seconduser)  
  await contracts.gratitude.newround({ authorization: `${gratitude}@active` })
  const contractBalanceAfter = await getBalance(gratitude)

  const secondBalanceAfter = await getBalance(seconduser)

  assert({
    given: 'gratitude round finished',
    should: 'contract balance',
    actual: contractBalanceAfter,
    expected: contractBalanceBefore
  })

  assert({
    given: 'gratitude round finished',
    should: 'second user received seeds',
    actual: secondBalanceAfter,
    expected: secondBalanceBefore + amount
  })

})
