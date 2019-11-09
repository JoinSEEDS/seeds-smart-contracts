const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts } = require('../scripts/helper')

const { harvest, accounts, proposals, settings, token, bank, firstuser, seconduser, thirduser } = names

describe('Proposals', async assert => {
  const contracts = await initContracts({ accounts, proposals, token, harvest, settings })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', { authorization: `${accounts}@active` })

  console.log('create proposal')
  await contracts.proposals.create(firstuser, firstuser, '100000.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', { authorization: `${firstuser}@active` })

  console.log('create another proposal')
  await contracts.proposals.create(seconduser, seconduser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', { authorization: `${seconduser}@active` })

  console.log('update proposal')
  await contracts.proposals.update(1, 'title2', 'summary2', 'description2', 'image2', 'url2', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '1000.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('deposit stake (without memo)')
  await contracts.token.transfer(seconduser, proposals, '1000.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 10, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 10, { authorization: `${proposals}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })

  try {
    await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })
  } catch (err) {
    console.log('favour first proposal (failed)')
  }

  try {
    await contracts.proposals.against(firstuser, 2, 1, { authorization: `${firstuser}@active` })
  } catch (err) {
    console.log('against second proposal (failed)')
  }

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  console.log('favour first proposal')
  await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })

  console.log('against second proposal')
  await contracts.proposals.against(firstuser, 2, 1, { authorization: `${firstuser}@active` })

  const balancesBefore = [
    await getBalance(firstuser),
    await getBalance(seconduser)
  ]

  console.log('execute proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  const balancesAfter = [
    await getBalance(firstuser),
    await getBalance(seconduser)
  ]

  const { rows } = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  delete rows[0].creation_date
  delete rows[1].creation_date

  assert({
    given: 'passed proposal',
    should: 'send reward and stake',
    actual: balancesAfter[0] - balancesBefore[0],
    expected: 1100
  })

  assert({
    given: 'rejected proposal',
    should: 'burn staked tokens',
    actual: balancesAfter[1] - balancesBefore[1],
    expected: 0
  })

  assert({
    given: 'passed proposal',
    should: 'show passed proposal',
    actual: rows[0],
    expected: {
      id: 1,
      creator: firstuser,
      recipient: firstuser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 1,
      total: 5,
      favour: 5,
      against: 0,
      stage: 'done',
      title: 'title2',
      summary: 'summary2',
      description: 'description2',
      image: 'image2',
      url: 'url2',
      status: 'passed'
    }
  })

  assert({
    given: 'rejected proposal',
    should: 'show rejected proposal',
    actual: rows[1],
    expected: {
      id: 2,
      creator: seconduser,
      recipient: seconduser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 1,
      favour: 0,
      against: 1,
      stage: 'done',
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected'
    }
  })
})
