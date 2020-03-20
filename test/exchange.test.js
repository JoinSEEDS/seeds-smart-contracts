const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, getBalance } = require('../scripts/helper.js')

const { token, accounts, tlostoken, exchange, firstuser } = names

describe('Exchange', async assert => {


  const contracts = await initContracts({ accounts, token, exchange })
  
  const tlosQuantity = '10.0000 TLOS'
  const seedsQuantity = '100.0000 SEEDS'
  
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  console.log(`reset exchange`)
  await contracts.exchange.reset({ authorization: `${exchange}@active` })  

  console.log(`update daily limits`)
  await contracts.exchange.updatelimit("2500.0000 SEEDS", "100.0000 SEEDS", "3.0000 SEEDS", { authorization: `${exchange}@active` })
  
  console.log(`update USD exchange rate`)
  await contracts.exchange.updateusd("101.0000 SEEDS", { authorization: `${exchange}@active` })

  console.log(`update TLOS rate`)
  await contracts.exchange.updatetlos("2.0000 SEEDS", { authorization: `${exchange}@active` })
  
  //console.log(`transfer ${tlosQuantity} from ${firstuser} to ${exchange}`)
  //await contracts.tlostoken.transfer(firstuser, exchange, tlosQuantity, '', { authorization: `${firstuser}@active` })

  let seeds


  await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgf", 99 * 10000, { authorization: `${exchange}@active` })


  console.log(`reset daily stats`)
  await contracts.exchange.onperiod({ authorization: `${exchange}@active` })  
  
  const seedsBalance = await getBalance(firstuser)
  
  assert({
    given: `sent ${tlosQuantity} to exchange`,
    should: `receive ${seedsQuantity} to account`,
    actual: seedsBalance,
    expected: Number.parseInt(seedsQuantity)
  })
})