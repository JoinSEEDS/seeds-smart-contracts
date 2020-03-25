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

  let soldBefore = await eos.getTableRows({
    code: exchange,
    scope: exchange,
    table: 'sold',
    json: true,
  })

  await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgf", parseInt(usd * 10000), { authorization: `${exchange}@active` })

  let balanceAfter = await getBalanceFloat(firstuser)

  let soldAfter = await eos.getTableRows({
    code: exchange,
    scope: exchange,
    table: 'sold',
    json: true,
  })

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

  assert({
    given: `sent ${usd} USD to exchange`,
    should: `receive ${expectedSeeds} to account going from `+balanceBefore + ' to ' + balanceAfter,
    actual: balanceAfter,
    expected: balanceBefore + expectedSeeds
  })

  assert({
    given: 'sent seeds',
    should: 'update sold',
    actual: soldAfter.rows[0].total_sold,
    expected: soldBefore.rows[0].total_sold + expectedSeeds * 10000
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

describe.only('Token Sale', async assert => {

  const contracts = await initContracts({ accounts, token, exchange })
  
  console.log(`reset exchange`)
  await contracts.exchange.reset({ authorization: `${exchange}@active` })  

  console.log(`reset accounts`)
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log(`add user`)
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  console.log(`transfer seeds to ${exchange}`)
  //await contracts.token.transfer(firstuser, exchange, "2000000.0000 SEEDS", 'unit test', { authorization: `${firstuser}@active` })

  console.log(`update daily limits`)
  await contracts.exchange.updatelimit("2500000.0000 SEEDS", "10000.0000 SEEDS", "3.0000 SEEDS", { authorization: `${exchange}@active` })
  
  console.log(`update TLOS rate - tlos per usd`)
  await contracts.exchange.updatetlos("3.0000 SEEDS", { authorization: `${exchange}@active` })
  
  //console.log(`transfer ${tlosQuantity} from ${firstuser} to ${exchange}`)
  //await contracts.tlostoken.transfer(firstuser, exchange, tlosQuantity, '', { authorization: `${firstuser}@active` })

  let usd = 0.0099
  let seeds_per_usd = 100
  let expectedSeeds = seeds_per_usd * usd

  //await contracts.exchange.newpayment(firstuser, "BTC", "0x3affgfaaaabbb", parseInt(usd * 10000), { authorization: `${exchange}@active` })

  usd = 99
    
  console.log(`init token sale rounds`)

  const getRounds = async () => {
    let rounds = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'rounds',
      json: true,
      limit: 100,
    })

    console.log("rounds "+JSON.stringify(rounds, null, 2))

    return rounds
  }

  await getRounds()

  console.log("test init rounds - 100 seeds per round")
  await contracts.exchange.initrounds( (100) * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })  

  let rounds = await getRounds()

  rounds = rounds.rows.map( ( item ) => { return { 
    round: item.id,
    seeds_per_usd: parseFloat(item.seeds_per_usd), 
    usd_rate: (1.0 / parseFloat(item.seeds_per_usd)).toFixed(5) } 
  })

  let usd_rates = rounds.map( ({usd_rate})  => usd_rate )

  console.log("rounds "+JSON.stringify(rounds, null, 2))

  console.log("test add round")

  console.log("test update price")

  console.log("test purchasing")


  assert({
    given: `rounds price with 3.3% cumulative increase `,
    should: `match values from spreadsheet`,
    actual: [usd_rates[0], usd_rates[1], usd_rates[29], usd_rates[49]],
    expected: [ "0.01100",  "0.01136", "0.02820", "0.05399"]
  })

})