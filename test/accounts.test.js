const { describe } = require('riteway')
const { eos } = require('./helper')
const { equals } = require('ramda')

// {"account":"accounts","auth":{"accounts":[{"permission":{"actor":"accounts","permission":"eosio.code"},"weight":1}],"keys":[{"key":"EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV","weight":1}],"threshold":1,"waits":[]},"parent":"","permission":"owner"}

const publicKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

describe('accounts', async assert => {
  const accounts = await eos.contract('accounts')

  await accounts.addapp('application', { authorization: 'accounts@active' })

  await accounts.addrequest('application', 'user1', publicKey, publicKey, { authorization: 'application@active' })
  
  await accounts.addrequest('application', 'user2', publicKey, publicKey, { authorization: 'application@active' })

  await accounts.fulfill('application', 'user1', { authorization: 'accounts@owner' })
  
  await accounts.fulfill('application', 'user2', { authorization: 'accounts@owner' })

  await accounts.adduser('user1', { authorization: 'accounts@active' })
  
  await accounts.adduser('user2', { authorization: 'accounts@active' })

  const users = await eos.getTableRows({
    code: 'accounts',
    scope: 'accounts',
    table: 'users',
    json: true,
  })
  
  const applications = await eos.getTableRows({
    code: 'accounts',
    scope: 'accounts',
    table: 'apps',
    json: true
  })
  
  assert({
    given: 'users table',
    should: 'show joined users',
    actual: users.rows,
    expected: [{
      account: 'user1'
    }, {
      account: 'user2'
    }]
  })
  
  assert({
    given: 'apps table',
    should: 'show joined applications',
    actual: applications.rows,
    expected: [{
      account: 'application'
    }]
  })
})