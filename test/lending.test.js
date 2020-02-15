const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, getBalance, getTelosBalance } = require('../scripts/helper.js')

const { token, tlostoken, lending, firstuser } = names

describe('Lending', async assert => {
  const contracts = await initContracts({ token, tlostoken, lending })
  
  const feePercent = 10
  const rate = 10
  const telosTransferAmount = 10
  const seedsTransferAmount = 100
  const telosRefundAmount = 9

  const telosTransferQuantity = `${telosTransferAmount}.0000 TLOS`
  const seedsTransferQuantity = `${seedsTransferAmount}.0000 SEEDS`
  const telosRefundQuantity = `${telosRefundAmount}.0000 TLOS`

  const seedsBalance = []
  const telosBalance = []

  seedsBalance.push(await getBalance(firstuser))
  telosBalance.push(await getTelosBalance(firstuser))

  console.log(`update fee percent to ${feePercent}`)
  await contracts.lending.updatefee(feePercent, { authorization: `${lending}@active` })
  
  console.log(`update lending rate to ${rate}`)
  await contracts.lending.updaterate(rate, { authorization: `${lending}@active` })
  
  console.log(`transfer ${telosTransferQuantity} from ${firstuser} to ${lending}`)
  await contracts.tlostoken.transfer(firstuser, lending, telosTransferQuantity, '', { authorization: `${firstuser}@active` })
  
  seedsBalance.push(await getBalance(firstuser))
  telosBalance.push(await getTelosBalance(firstuser))
  
  console.log(`transfer ${seedsTransferQuantity} from ${firstuser} to ${lending}`)
  await contracts.token.transfer(firstuser, lending, seedsTransferQuantity, '', { authorization: `${firstuser}@active` })

  seedsBalance.push(await getBalance(firstuser))
  telosBalance.push(await getTelosBalance(firstuser))

  assert({
    given: `sent ${telosTransferQuantity} to lending`,
    should: `receive ${seedsTransferQuantity} to account`,
    actual: seedsBalance[1] - seedsBalance[0],
    expected: +seedsTransferAmount
  })

  assert({
    given: `sent ${telosTransferQuantity} to lending`,
    should: `decrease ${telosTransferQuantity} from account`,
    actual: telosBalance[1] - telosBalance[0],
    expected: -telosTransferAmount
  })

  assert({
    given: `sent ${seedsTransferQuantity} to lending`,
    should: `receive ${telosRefundQuantity} to account`,
    actual: telosTransferAmount[2] - telosTransferAmount[1],
    expected: +telosRefundAmount
  })

  assert({
    given: `sent ${seedsTransferQuantity} to lending`,
    should: `decrease ${seedsTransferQuantity} from account`,
    actual: seedsBalance[2] - seedsBalance[1],
    expected: -seedsTransferAmount
  })
})