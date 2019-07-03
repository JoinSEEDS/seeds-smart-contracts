const { describe } = require('riteway')
const { eos, names } = require('../scripts/helper')
const { equals } = require('ramda')

const publicKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

const { accounts, harvest, token, application, firstuser, seconduser } = names

describe('accounts', async assert => {
  const contract = await eos.contract(accounts)
  const thetoken = await eos.contract(token)

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('join application')
  await contract.addapp(application, { authorization: `${accounts}@active` })

  try {
    console.log(`create account ${firstuser}`)
    await contract.addrequest(application, firstuser, publicKey, publicKey, { authorization: `${application}@active` })
    await contract.fulfill(application, firstuser, { authorization: `${accounts}@owner` })
  } catch (err) {
    console.log(`${firstuser} account already exists`)
  }

  try {
    console.log(`create account ${seconduser}`)
    await contract.addrequest(application, seconduser, publicKey, publicKey, { authorization: `${application}@active` })
    await contract.fulfill(application, seconduser, { authorization: `${accounts}@owner` })
  } catch (err) {
    console.log(`${seconduser} account already exists`)
  }

  console.log('join users')
  await contract.adduser(firstuser, { authorization: `${accounts}@active` })
  await contract.adduser(seconduser, { authorization: `${accounts}@active` })

  console.log('plant 50 seeds')
  await thetoken.transfer(firstuser, harvest, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('make resident')
  await contract.makeresident(firstuser, { authorization: `${firstuser}@active` })

  console.log("plant 100 seeds")
  await thetoken.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('make citizen')
  await contract.makecitizen(firstuser, { authorization: `${firstuser}@active` })

  console.log('set name')
  await contract.setname(firstuser, 'First User', { authorization: `${firstuser}@active` })
  await contract.setname(seconduser, 'Second User', { authorization: `${seconduser}@active` })

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const applications = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'apps',
    json: true
  })

  assert({
    given: 'users table',
    should: 'show joined users',
    actual: users.rows.map(({ account, status, fullname }) => ({ account, status, fullname })),
    expected: [{
      account: firstuser,
      status: 'citizen',
      fullname: 'First User'
    }, {
      account: seconduser,
      status: 'visitor',
      fullname: 'Second User'
    }]
  })
  
  assert({
    given: 'random reputation',
    should: 'less than 99',
    actual: users.rows.map(({ reputation }) => reputation < 99),
    expected: [true, true]
  })

  assert({
    given: 'apps table',
    should: 'show joined applications',
    actual: applications.rows,
    expected: [{
      account: application
    }]
  })
})
