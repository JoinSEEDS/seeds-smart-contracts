const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, getBalanceFloat } = require('../scripts/helper.js')

const { token, accounts, tlostoken, exchange, firstuser } = names

describe('Exchange', async assert => {


  const contracts = await initContracts({ accounts, token, exchange })
  
  const tlosQuantity = '10.0000 TLOS'
  const seedsQuantity = '100.0000 SEEDS'
  

  console.log(`reset exchange`)
  await contracts.exchange.reset({ authorization: `${exchange}@active` })  

  console.log(`transfer seeds to ${exchange}`)
  await contracts.token.transfer(firstuser, exchange, "20.0000 SEEDS", 'unit test', { authorization: `${firstuser}@active` })

  console.log(`reset accounts`)
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log(`add user`)
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  console.log(`update daily limits`)
  await contracts.exchange.updatelimit("2500.0000 SEEDS", "100.0000 SEEDS", "3.0000 SEEDS", { authorization: `${exchange}@active` })
  
  console.log(`update USD exchange rate - seeds per USD`)
  let seeds_per_usd = 101
  await contracts.exchange.updateusd(""+seeds_per_usd + ".0000 SEEDS", { authorization: `${exchange}@active` })

  console.log(`update TLOS rate - tlos per usd`)
  await contracts.exchange.updatetlos("3.0000 SEEDS", { authorization: `${exchange}@active` })
  
  //console.log(`transfer ${tlosQuantity} from ${firstuser} to ${exchange}`)
  //await contracts.tlostoken.transfer(firstuser, exchange, tlosQuantity, '', { authorization: `${firstuser}@active` })

  let usd = 0.0099

  let expectedSeeds = seeds_per_usd * usd

  console.log("expected: "+expectedSeeds)

  let balanceBefore = await getBalanceFloat(firstuser)

  console.log("balance before "+balanceBefore)

  await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgf", parseInt(usd * 10000), { authorization: `${exchange}@active` })

  let balanceAfter = await getBalanceFloat(firstuser)

  console.log("balance before "+balanceAfter)

  usd = 99

  let canExceedBalance = false
  try {
    await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgf", usd * 10000, { authorization: `${exchange}@active` })
    canExceedBalance = true
  } catch (err) {
  }

  let allowDuplicateTransaction = false
  try {
    await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgf", parseInt(usd * 10000), { authorization: `${exchange}@active` })
    allowDuplicateTransaction = true
  } catch (err) {
  }



  console.log(`reset daily stats`)
  await contracts.exchange.onperiod({ authorization: `${exchange}@active` })  
  
  const seedsBalance = await getBalanceFloat(firstuser)
  
  assert({
    given: `sent ${usd} USD to exchange`,
    should: `receive ${expectedSeeds} to account going from `+balanceBefore + ' to ' + balanceAfter,
    actual: balanceAfter,
    expected: balanceBefore + expectedSeeds
  })

  assert({
    given: `user buys more than they are allowed`,
    should: `fail`,
    actual: canExceedBalance,
    expected: false
  })


  assert({
    given: `newpurchase called multiple times with same transaction`,
    should: `fail`,
    actual: allowDuplicateTransaction,
    expected: false
  })

  // TLOS test not quite working
  // assert({
  //   given: `sent ${tlosQuantity} to exchange`,
  //   should: `receive ${seedsQuantity} to account`,
  //   actual: seedsBalance,
  //   expected: Number.parseInt(seedsQuantity)
  // })
})