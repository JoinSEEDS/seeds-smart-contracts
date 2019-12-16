const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, getBalance } = require('../scripts/helper.js')

const { token, tlostoken, exchange, firstuser } = names

describe('Exchange', async assert => {
  const contracts = await initContracts({ token, tlostoken, exchange })
  
  const limit = 100
  const rate = 10
  const tlosQuantity = '10.0000 TLOS'
  const seedsQuantity = '100.0000 SEEDS'
  
  console.log(`update daily limit to ${limit}`)
  await contracts.exchange.updatelimit(limit, { authorization: `${exchange}@active` })
  
  console.log(`update exchange rate to ${rate}`)
  await contracts.exchange.updaterate(rate, { authorization: `${exchange}@active` })
  
  console.log(`transfer ${tlosQuantity} from ${firstuser} to ${exchange}`)
  await contracts.tlostoken.transfer(firstuser, exchange, tlosQuantity, '', { authorization: `${firstuser}@active` })

  console.log(`reset daily stats`)
  await contracts.exchange.dailyreset({ authorization: `${exchange}@active` })  
  
  const seedsBalance = await getBalance(firstuser)
  
  assert({
    given: `sent ${tlosQuantity} to exchange`,
    should: `receive ${seedsQuantity} to account`,
    actual: seedsBalance,
    expected: Number.parseInt(seedsQuantity)
  })
})