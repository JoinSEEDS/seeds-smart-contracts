const { describe } = require('riteway')
const { eos, names, getTableRows } = require('../scripts/helper')

const { accounts, harvest, token, invites, firstuser, seconduser, thirduser } = names

const publicKey = 'EOS7iYzR2MmQnGga7iD2rPzvm5mEFXx6L1pjFTQYKRtdfDcG9NTTU'

const initContracts = (contractAccounts) =>
  Promise.all(
    Object.keys(contractAccounts)
      .map(contractName =>
        eos.contract(contractAccounts[contractName])
      )
  )
  .then(contractsArr => contractsArr.reduce(
    (acc, cur, idx) => ({
      ...acc,
      [Object.keys(contractAccounts)[idx]]: cur
    }),
    {}
  ))

const randomAccountName = () => Math.random().toString(36).substring(2).replace(/\d/g, '').toString()

describe('Invites', async assert => {
  const contracts = await initContracts({ accounts, invites, token })

  const inviteduser = randomAccountName()

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset invites')
  await contracts.invites.reset({ authorization: `${invites}@active` })

  const sponsorsInitial = await getTableRows({
    code: invites,
    scope: invites,
    table: 'sponsors',
    json: true
  })

  console.log(`send invite from ${firstuser}`)
  await contracts.token.transfer(firstuser, invites, '12.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  const sponsorsInvited = await getTableRows({
    code: invites,
    scope: invites,
    table: 'sponsors',
    json: true
  })

  console.log(`accept invite from ${inviteduser}`)
  await contracts.invites.accept(firstuser, inviteduser, publicKey, '12.0000 SEEDS', { authorization: `${invites}@api` })

  const nonTelosAccount = randomAccountName()

  console.log(`accept invite from ${inviteduser} with 0 seeds non account`)
  var hasError = false
  try {
    await contracts.invites.accept(firstuser, nonTelosAccount, publicKey, '0.0000 SEEDS', { authorization: `${invites}@api` })
  } catch (err) {
    hasError = true
  }

  assert({
    given: 'random account invite with 0 seeds',
    should: 'fail',
    actual: hasError,
    expected: true
  })

  console.log(`accept invite from ${inviteduser} with 0 seeds Telos account`)
  hasError = false
  try {
    await contracts.invites.accept(firstuser, thirduser, publicKey, '0.0000 SEEDS', { authorization: `${invites}@api` })
  } catch (err) {
    hasError = true
  }

  assert({
    given: 'Telos account creation with 0 seeds',
    should: 'succeed',
    actual: hasError,
    expected: false
  })

  const sponsorsClaimed = await getTableRows({
    code: invites,
    scope: invites,
    table: 'sponsors',
    json: true
  })

  const accountsClaimed = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true
  })

  let accountsClaimedNames = accountsClaimed.rows.map( (item) => item.account ).sort()

console.log("accountsClaimed: "+JSON.stringify(accountsClaimedNames, null, 2))

  const harvestClaimed = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'balances',
    json: true
  })

  const tokenClaimed = await getTableRows({
    code: token,
    scope: inviteduser,
    table: 'accounts',
    json: true
  })

  assert({
    given: 'sponsors before invited',
    should: 'be empty table',
    actual: sponsorsInitial.rows,
    expected: []
  })

  assert({
    given: 'sponsors after invited',
    should: 'be sponsor with positive balance',
    actual: sponsorsInvited.rows,
    expected: [{
      account: firstuser,
      balance: '12.0000 SEEDS'
    }]
  })

  assert({
    given: 'sponsors after claimed',
    should: 'be sponsor with zero balance',
    actual: sponsorsClaimed.rows,
    expected: [{
      account: firstuser,
      balance: '0.0000 SEEDS'
    }]
  })

  const acctAfterClaimed = [
    inviteduser,
    thirduser
  ].sort()
  assert({
    given: 'accounts after claimed',
    should: 'be created account',
    actual: accountsClaimedNames,
    expected: acctAfterClaimed
  })


  assert({
    given: 'planted balance after claimed',
    should: 'be positive amount',
    actual: harvestClaimed.rows.find(user => user.account == inviteduser),
    expected: {
      account: inviteduser,
      planted: '5.0000 SEEDS',
      reward: '0.0000 SEEDS'
    }
  })

  assert({
    given: 'current balance after claimed',
    should: 'be positive amount',
    actual: tokenClaimed.rows,
    expected: [{
      balance: '7.0000 SEEDS'
    }]
  })
})
