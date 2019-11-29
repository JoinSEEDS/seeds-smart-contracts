const { describe } = require('riteway')
const { eos, names, isLocal } = require('../scripts/helper')
const { equals } = require('ramda')

const publicKey = 'EOS7iYzR2MmQnGga7iD2rPzvm5mEFXx6L1pjFTQYKRtdfDcG9NTTU'

const { accounts, harvest, token, application, firstuser, seconduser } = names

describe('account creation', async assert => {
  const contract = await eos.contract(accounts)

  const newuser = 'newusername'

  await contract.addrequest(application, newuser, publicKey, publicKey, { authorization: `${application}@active` })
  await contract.fulfill(application, newuser, { authorization: `${accounts}@owner` })

  const { required_auth: { keys } } =
    (await eos.getAccount(newuser))
      .permissions.find(p => p.perm_name == 'active')

  assert({
    given: 'created user',
    should: 'have correct public key',
    actual: keys,
    expected: [{
      key: publicKey,
      weight: 1
    }]
  })
})

describe.only('accounts', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(accounts)
  const thetoken = await eos.contract(token)

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await thetoken.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contract.adduser(firstuser, 'First user', { authorization: `${accounts}@active` })
  await contract.joinuser(seconduser, { authorization: `${seconduser}@active` })

  console.log('plant 50 seeds')
  await thetoken.transfer(firstuser, harvest, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log("plant 100 seeds")
  await thetoken.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('add referral')
  await contract.addref(firstuser, seconduser, { authorization: `${accounts}@api` })

  console.log('update reputation')
  await contract.addrep(firstuser, 100, { authorization: `${accounts}@api` })
  await contract.subrep(seconduser, 1, { authorization: `${accounts}@api` })

  console.log('test citizen')
  await contract.testcitizen(firstuser, { authorization: `${accounts}@active` })

  console.log('test resident')
  await contract.testresident(seconduser, { authorization: `${accounts}@active` })

  try {
    console.log('make citizen')
    await contract.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    console.log('user not ready to become citizen')
  }

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const refs = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'refs',
    json: true
  })

  const reps = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'reputation',
    json: true
  })

  const now = new Date() / 1000

  const firstTimestamp = users.rows[0].timestamp

  assert({
    given: 'changed reputation',
    should: 'have correct values',
    actual: reps.rows.map(({ reputation }) => reputation),
    expected: [100, 0]
  })

  assert({
    given: 'created user',
    should: 'have correct timestamp',
    actual: Math.abs(firstTimestamp - now) < 5,
    expected: true
  })

  assert({
    given: 'invited user',
    should: 'have row in table',
    actual: refs,
    expected: {
      rows: [{
        referrer: firstuser,
        invited: seconduser
      }],
      more: false
    }
  })

  assert({
    given: 'users table',
    should: 'show joined users',
    actual: users.rows.map(({ account, status, nickname, reputation }) => ({ account, status, nickname, reputation })),
    expected: [{
      account: firstuser,
      status: 'citizen',
      nickname: 'First user',
      reputation: 100,
    }, {
      account: seconduser,
      status: 'resident',
      nickname: '',
      reputation: 0
    }]
  })
})
