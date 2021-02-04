const { describe } = require("riteway")
const { names, getTableRows, isLocal, initContracts, sha256, ramdom64ByteHexString } = require("../scripts/helper")

const { firstuser, seconduser, accounts, token, service, onboarding } = names

const fromHexString = hexString =>
  new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))

describe('service test', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, service, onboarding, token })

  const getSeedsBalance = async account => {
    const balanceTable = await getTableRows({
      code: token,
      scope: account,
      table: 'accounts',
      json: true
    })

    console.log("seeds balance "+JSON.stringify(balanceTable, null, 2))

    const balance = balanceTable.rows
    return parseInt(balance[0].balance)
  }

  const getBalance = async account => {

    console.log("get balance "+account)

    const balanceTable = await getTableRows({
      code: service,
      scope: service,
      table: 'balances',
      json: true
    })

    console.log("balance for "+account+" "+ JSON.stringify(balanceTable, null, 2))

    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].balance)
  }

  const checkBalance = async (account, expectedBalance) => {
    const balance = await getBalance(account)
    assert({
      given: `${account} used the service`,
      should: 'have the correct balance',
      actual: balance,
      expected: expectedBalance
    })
  }

  console.log('service reset')
  await contracts.service.reset({ authorization: `${service}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('onboarding reset')
  await contracts.onboarding.reset({ authorization: `${onboarding}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  const users = [firstuser, seconduser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
  }

  const amount = 1000

  const balanceBefore = 0

  console.log('transfer to service')
  for (const user of users) {
    await contracts.token.transfer(user, service, `${amount}.0000 SEEDS`, 'test', { authorization: `${user}@active` })
    await checkBalance(user, amount)
  }

  await checkBalance(service, amount * users.length)

  const balanceAfter = await getBalance(service)

  const seconduserBalanceBefore = await getBalance(seconduser)

  console.log('pause')
  await contracts.service.pause(firstuser, { authorization: `${firstuser}@active` })

  const inviteHash = "18b81e2358262a8f20f7ff909d76cc169bc8d1fa480a31c72be873fe45706c4a"

  let transferWhenPause = false
  try {
    await contracts.service.createinvite(firstuser, "10.0000 SEEDS", "5.0000 SEEDS", inviteHash, { authorization: `${service}@invite` })
    transferWhenPause = true
  } catch (err) {
    console.log('can not transfer when account is paused (expected)')
  }

  console.log('unpause')
  await contracts.service.unpause(firstuser, { authorization: `${firstuser}@active` })

  console.log('invite')
  const transferAmount = 15
  await contracts.service.createinvite(firstuser, "10.0000 SEEDS", "5.0000 SEEDS", inviteHash, { authorization: `${service}@invite` })

  console.log("check for valid invite")
  
  const balanceAfter2 = await getBalance(service)

  const invites = await getTableRows({
    code: onboarding,
    scope: onboarding,
    table: 'invites',
    json: true,
  })

  console.log("invites: "+JSON.stringify(invites, null, 2))

  assert({
    given: 'invite created via service',
    should: 'has invite',
    actual: invites.rows,
    expected: [
      {
        "invite_id": 0,
        "transfer_quantity": "10.0000 SEEDS",
        "sow_quantity": "5.0000 SEEDS",
        "sponsor": "hello.seeds",
        "account": "",
        "invite_hash": inviteHash,
        "invite_secret": "0000000000000000000000000000000000000000000000000000000000000000"
      }
  ]
  })

  await checkBalance(firstuser, amount - transferAmount)
  await checkBalance(service, users.length * amount - transferAmount)

  const expected = amount * users.length

  const actual = balanceAfter - balanceBefore

  console.log(" 111 asdsadsadad");
  assert({
    given: 'transfer to service',
    should: 'have more balance',
    actual: actual,
    expected: expected
  })
  console.log(" 222 asdsadsadad");

  assert({
    given: 'invite user',
    should: 'have less balance',
    actual: balanceAfter - balanceAfter2,
    expected:  transferAmount
  })
  console.log(" 333 asdsadsadad");

  assert({
    given: 'account paused',
    should: 'not be able to transfer',
    actual: transferWhenPause,
    expected: false
  })


})

