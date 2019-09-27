const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts } = require('../scripts/helper')

const { harvest, accounts, proposals, token, bank, firstuser, seconduser, thirduser } = names

describe('Proposals', async assert => {
  const contracts = await initContracts({ accounts, proposals, token, harvest })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', { authorization: `${accounts}@active` })

  console.log('deposit bank')
  await contracts.token.transfer(firstuser, bank, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('create proposal')
  await contracts.proposals.create(firstuser, seconduser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', { authorization: `${firstuser}@active` })

  console.log('update proposal')
  await contracts.proposals.update(1, 'title2', 'summary2', 'description2', 'image2', 'url2', { authorization: `${firstuser}@active` })

  const props = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })
  
  const createdProposal = props.rows[1]
  delete createdProposal.creation_date

  assert({
    given: 'proposals table',
    should: 'show created proposal',
    actual: createdProposal,
    expected: {
      id: 1,
      creator: firstuser,
      recipient: seconduser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 0,
      favour: 0,
      against: 0,
      title: 'title2',
      summary: 'summary2',
      description: 'description2',
      image: 'image2',
      url: 'url2',
      status: 'open'
    }
  })

  console.log('stake seeds')
  await contracts.token.transfer(firstuser, proposals, '100.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 10, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 10, { authorization: `${proposals}@active` })

  console.log('favour vote')
  await contracts.proposals.favour(firstuser, 1, 9, { authorization: `${firstuser}@active` })

  console.log('against vote')
  await contracts.proposals.against(seconduser, 1, 8, { authorization: `${seconduser}@active` })

  console.log('execute proposal')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  const secondUserFinalBalance = await getBalance(seconduser)

  assert({
    given: 'proposal executed',
    should: 'increase recipient balance',
    actual: secondUserFinalBalance > secondUserInitialBalance,
    expected: true
  })
})
