const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, initContracts, getBalance, createKeypair } = require("../scripts/helper")
const eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

const { firstuser, seconduser, pouch, accounts, token, bank } = names

const createKeyPermission = async (account, role, parentRole = 'active', key) => {

  const { permissions } = await eos.getAccount(account)

  const perm = permissions.find(p => p.perm_name === role)

  if (perm) {
    const { parent, required_auth } = perm
    const { keys } = required_auth

    if (keys.find(item => item.key === key)) {
      console.log("- createKeyPermission key already exists "+key)
      return;
    }  
  }

  await eos.updateauth({
    account,
    permission: role,
    parent: parentRole,
    auth: {
      threshold: 1,
      waits: [],
      accounts: [],
      keys: [{
        key,
        weight: 1
      }]
    }
  }, { authorization: `${account}@owner` })
}

const linkAuth = async (account, role, contract, action, { actor, perm }) => {
  try {
    await eos.linkauth({
      account,
      code: contract,
      type: action,
      requirement: role
    }, { authorization: `${actor}@${perm}` })
  } catch (err) {
    console.log('- linkAuth already exists')
  }
}

describe('pouch general', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ pouch, accounts, token })

  const getPouchBalance = async account => {
    const balanceTable = await getTableRows({
      code: pouch,
      scope: pouch,
      table: 'balances',
      json: true
    })
    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].balance)
  }

  const checkPouchBalance = async (account, expectedBalance) => {
    const balance = await getPouchBalance(account)
    assert({
      given: `${account} used the pouch`,
      should: 'have the correct balance',
      actual: balance,
      expected: expectedBalance
    })
  }

  console.log('pouch reset')
  await contracts.pouch.reset({ authorization: `${pouch}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  const users = [firstuser, seconduser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
  }

  const amount = 1000

  const bankBalanceBefore = await getBalance(bank) || 0

  console.log('transfer to pouch')
  for (const user of users) {
    await contracts.token.transfer(user, pouch, `${amount}.0000 SEEDS`, 'test', { authorization: `${user}@active` })
    await checkPouchBalance(user, amount)
  }

  await checkPouchBalance(pouch, amount * users.length)

  const bankBalanceAfter = await getBalance(bank)

  console.log('withdraw')
  const withdrawAmount = 500
  
  await contracts.pouch.withdraw(seconduser, `${withdrawAmount}.0000 SEEDS`, { authorization: `${seconduser}@active` })
  await checkPouchBalance(seconduser, amount - withdrawAmount)

  const seconduserBalanceBefore = await getBalance(seconduser)

  console.log('freeze')
  await contracts.pouch.freeze(firstuser, { authorization: `${firstuser}@active` })

  let transferWhenFreeze = false
  try {
    await contracts.pouch.transfer(firstuser, seconduser, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })
    transferWhenFreeze = true
  } catch (err) {
    console.log('can not transfer when account is freezed (expected)')
  }

  console.log('unfreeze')
  await contracts.pouch.unfreeze(firstuser, { authorization: `${firstuser}@active` })

  console.log('create pouch permission')
  await createKeyPermission(firstuser, 'pouch', 'active', eosDevKey)
  await linkAuth(firstuser, 'pouch', pouch, 'transfer', { actor: firstuser, perm: 'active' })

  console.log('transfer')
  const transferAmount = 100
  await contracts.pouch.transfer(firstuser, seconduser, `${transferAmount}.0000 SEEDS`, '', { authorization: `${firstuser}@pouch` })

  const seconduserBalanceAfter = await getBalance(seconduser)
  const bankBalanceAfter2 = await getBalance(bank)

  await checkPouchBalance(firstuser, amount - transferAmount)
  await checkPouchBalance(pouch, users.length * amount - transferAmount - withdrawAmount)

  assert({
    given: 'transfer to pouch',
    should: 'have more balance',
    actual: bankBalanceAfter - bankBalanceBefore,
    expected: amount * users.length
  })

  assert({
    given: 'transfer to another user',
    should: 'have less balance',
    actual: bankBalanceAfter - bankBalanceAfter2,
    expected: transferAmount + withdrawAmount
  })

  assert({
    given: 'account freezed',
    should: 'not be able to transfer',
    actual: transferWhenFreeze,
    expected: false
  })

  assert({
    given: 'transfer called',
    should: 'transfer seeds to account',
    actual: seconduserBalanceAfter - seconduserBalanceBefore,
    expected: 100
  })

})

