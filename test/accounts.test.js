const { describe } = require('riteway')
const { eos, names, isLocal } = require('../scripts/helper')
const { equals } = require('ramda')

const publicKey = 'EOS7iYzR2MmQnGga7iD2rPzvm5mEFXx6L1pjFTQYKRtdfDcG9NTTU'

const { accounts, harvest, token, application, firstuser, seconduser, thirduser } = names

describe('genesis testing', async assert => {
  const contract = await eos.contract(accounts)

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('test genesis')
  await contract.adduser(thirduser, 'First user', { authorization: `${accounts}@active` })
  await contract.testcitizen(thirduser, { authorization: `${accounts}@active` })

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  let user = users.rows[0]

  assert({
    given: 'genesis',
    should: 'be citizen',
    actual: user.status,
    expected: "citizen"
  })

})

describe('accounts', async assert => {

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

  console.log('add users')
  await contract.adduser(firstuser, 'First user', { authorization: `${accounts}@active` })
  await contract.adduser(seconduser, 'Second user', { authorization: `${accounts}@active` })

  console.log('plant 50 seeds')
  await thetoken.transfer(firstuser, harvest, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log("plant 100 seeds")
  await thetoken.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('add referral')
  await contract.addref(firstuser, seconduser, { authorization: `${accounts}@api` })

  console.log('update reputation')
  await contract.addrep(firstuser, 100, { authorization: `${accounts}@api` })
  await contract.subrep(seconduser, 1, { authorization: `${accounts}@api` })

  console.log('update user data')
  await contract.update(
    firstuser, 
    "individual", 
    "Ricky G",
    "https://m.media-amazon.com/images/M/MV5BMjQzOTEzMTk1M15BMl5BanBnXkFtZTgwODI1Mzc0MDI@._V1_.jpg",
    "I'm from the UK",
    "Roless... hmmm... ",
    "Skills: Making jokes. Some acting. Offending people.",
    "Animals",
    { authorization: `${firstuser}@active` })


  try {
    console.log('make resident')

    await contract.makeresident(firstuser, { authorization: `${firstuser}@active` })

    console.log('make citizen')
    
    await contract.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    console.log('user not ready to become citizen' + err)
  }

  console.log('test citizen')
  await contract.testcitizen(firstuser, { authorization: `${accounts}@active` })

  console.log('add vouch')
  await contract.vouch(firstuser, seconduser, { authorization: `${firstuser}@active` })

  console.log('test resident')
  await contract.testresident(seconduser, { authorization: `${accounts}@active` })

  const vouch = await eos.getTableRows({
    code: accounts,
    scope: seconduser,
    table: 'vouch',
    json: true
  })

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

  console.log('test testremove')
  await contract.testremove(seconduser, { authorization: `${accounts}@active` })

  const usersAfterRemove = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true,
  })

  const vouchAfterRemove = await eos.getTableRows({
    code: accounts,
    scope: seconduser,
    table: 'vouch',
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
    given: 'changed inviter community building score',
    should: 'have correct values',
    actual: usersAfterRemove.rows.map(({ community_building_score }) => community_building_score),
    expected: [1]
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
      nickname: 'Ricky G',
      reputation: 100 + 1,
    }, {
      account: seconduser,
      status: 'resident',
      nickname: 'Second user',
      reputation: 10 // 10 because they got vouched for by a citizen
    }]
  })

  assert({
    given: 'referral',
    should: 'have entry in vouch table',
    actual: vouch.rows.length,
    expected: 1
  })

  assert({
    given: 'test-removed user',
    should: 'have 1 fewer users than before',
    actual: usersAfterRemove.rows.length,
    expected: users.rows.length - 1
  })
  assert({
    given: 'deleted user',
    should: 'no entry in vouch table',
    actual: vouchAfterRemove.rows.length,
    expected: 0
  })

})

