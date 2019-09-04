const { describe } = require('riteway')
const R = require('ramda')
const { names, getTableRows, getBalance, initContracts } = require('../scripts/helper')

const { accounts, proposals, token, bank, firstuser, seconduser, thirduser } = names

describe('Proposals', async assert => {
  const contracts = await initContracts({ accounts, proposals, token })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, { authorization: `${accounts}@active` })

  console.log('deposit bank')
  await contracts.token.transfer(firstuser, bank, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('create proposal')
  await contracts.proposals.create(firstuser, seconduser, '100.0000 SEEDS', 'donate for charity', { authorization: `${firstuser}@active` })

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

  const props = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  const secondUserFinalBalance = await getBalance(seconduser)

  assert({
    given: 'proposals table',
    should: 'show created proposal',
    actual: props.rows[1],
    expected: {
      id: 1,
      creator: firstuser,
      recipient: seconduser,
      quantity: '100.0000 SEEDS',
      memo: 'donate for charity',
      executed: 0,
      votes: 0
    }
  })

  assert({
    given: 'proposal executed',
    should: 'increase recipient balance',
    actual: secondUserFinalBalance > secondUserInitialBalance,
    expected: true
  })
})
