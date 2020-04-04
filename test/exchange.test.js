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
  
  //console.log(`update USD exchange rate - seeds per USD`)
  let seeds_per_usd = 100
  //await contracts.exchange.updateusd(""+seeds_per_usd + ".0000 SEEDS", { authorization: `${exchange}@active` })

  console.log(`update TLOS rate - tlos per usd`)
  await contracts.exchange.updatetlos("3.0000 SEEDS", { authorization: `${exchange}@active` })

  await contracts.exchange.onperiod({ authorization: `${exchange}@active` })

  await contracts.exchange.initrounds( (1100000) * 10000, "100.0000 SEEDS", { authorization: `${exchange}@active` })  

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

  console.log("balance after "+balanceAfter)

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

  expectedSeeds = parseFloat(expectedSeeds.toFixed(4))

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

describe('Token Sale Rounds', async assert => {

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
      
  console.log(`init token sale rounds`)

  const getRounds = async () => {
    let rounds = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'rounds',
      json: true,
      limit: 100,
    })

    //console.log("rounds "+JSON.stringify(rounds, null, 2))

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

  //console.log("rounds "+JSON.stringify(rounds, null, 2))

  let vol = (444) * 10000
  console.log("test add round")
  await contracts.exchange.addround( vol, "1.1000 SEEDS", { authorization: `${exchange}@active` })  

  let roundsAfterAdd = await getRounds()

  assert({
    given: `rounds price with 3.3% cumulative increase `,
    should: `match values from spreadsheet`,
    actual: [usd_rates[0], usd_rates[1], usd_rates[29], usd_rates[49]],
    expected: [ "0.01100",  "0.01136", "0.02820", "0.05399"]
  })

  assert({
    given: `add round `,
    should: `be appended to rounds`,
    actual: roundsAfterAdd.rows[50],
    expected: {
      "id": 50,
      "max_sold": 50000000 + vol,
      "seeds_per_usd": "1.1000 SEEDS"
    }
  })


})

describe('Basic Init Check', async assert => {

  const contracts = await initContracts({ accounts, token, exchange })
  
  console.log(`reset exchange`)
  await contracts.exchange.reset({ authorization: `${exchange}@active` })  

  console.log(`init token sale rounds`)
  await contracts.exchange.initsale({ authorization: `${exchange}@active` })
  
  let rounds = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'rounds',
      json: true,
      limit: 100,
  })
  
  let usd_per_seeds = 0.011
  let volume = 1100000
  let results = []
  for (let i=0; i<50; i++) {
    let seeds_per_usd = 1 / usd_per_seeds

    if (i==43) {
      console.log("Round 44: usd_per_seeds: "+usd_per_seeds + " seeds_per_usd: "+seeds_per_usd)
      // note: 225057.49999 is rounded by c++ to 225058 - doesn't matter given our precision requirements of 4 digits.
    }
    results.push({
      volume: (volume * 10000)+"",
      price: (Math.round(seeds_per_usd*10000.0)/10000.0).toFixed(4)
    })

    usd_per_seeds = usd_per_seeds * 1.033
    volume += 1100000
  }
  for(let i=0; i<results.length; i++) {
    assert({
      given: 'round '+i,
      should: 'have seeds per usd: ' + results[i].price + " SEEDS   Volume: "+results[i].volume,
      actual: { 
        price: rounds.rows[i].seeds_per_usd,
        volume: rounds.rows[i].max_sold
      },
      expected: {
        price: results[i].price + " SEEDS",
        volume: results[i].volume
      }
    })
    }



})
const getRounds = async () => {
  let rounds = await eos.getTableRows({
    code: exchange,
    scope: exchange,
    table: 'rounds',
    json: true,
    limit: 100,
  })
  return rounds
}

describe('Token Sale Price', async assert => {

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
  await contracts.exchange.updatelimit("100.0000 SEEDS", "10.0000 SEEDS", "3000000.0000 SEEDS", { authorization: `${exchange}@active` })
  
  console.log(`update TLOS rate - tlos per usd`)
  await contracts.exchange.updatetlos("3.0000 SEEDS", { authorization: `${exchange}@active` })
  //await contracts.exchange.updateusd("1.0000 SEEDS", { authorization: `${exchange}@active` })

  console.log(`init token sale rounds`)

  console.log("test init rounds - 10.0000 seeds per round")

  for (let i=0; i<12; i++) {
    await contracts.exchange.addround( 10 * 10000, i+1+".0000 SEEDS", { authorization: `${exchange}@active` })  
  }

  //console.log("rounds "+JSON.stringify(await getRounds(), null, 2))

  let b1 = await getBalanceFloat(firstuser)

  const updateprice = async (expected_round, expected_price, expected_remaining) => {

    let price = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'price',
      json: true,
    })
    //console.log("price: "+JSON.stringify(price, null, 2))

    assert({
      given: `new price`,
      should: `expected round: `+expected_round + " price: "+expected_price + " remaining "+expected_remaining,
      actual: price.rows[0],
      expected: {
        "id": 0,
        "current_round_id": expected_round,
        "current_seeds_per_usd": parseInt(expected_price)+".0000 SEEDS",
        "remaining": expected_remaining
      }
    })

  }

  const check = async (initial, expectedDelta, usd) => {
    let balance = await getBalanceFloat(firstuser)
    let delta = balance - initial
    //console.log(" expected "+expectedDelta + " actual " + delta + " initial "+ initial + " balance: "+ balance)

    assert({
      given: `purchase `+usd+" usd worth",
      should: `receive expected number of Seeds: `+expectedDelta,
      actual: delta.toFixed(4),
      expected: expectedDelta.toFixed(4)
    })
  
  }

  console.log("buy seeds 10 USD")

  // edge case, buy entire round, check price after
  await contracts.exchange.newpayment(firstuser, "BTC", "0", parseInt(10 * 10000), { authorization: `${exchange}@active` })

  let seedsBalance = 10

  await check(b1, seedsBalance, 10)

  b1 += seedsBalance

  await updateprice(1, 2, 10 * 10000)


  console.log("buy seeds 0.5 USD")

  await contracts.exchange.newpayment(firstuser, "BTC", "01", parseInt(5000), { authorization: `${exchange}@active` })

  await check(b1, 1, 0.5)

  b1 += 1

  await updateprice(1, 2, 9 * 10000)

  console.log("buy seeds 6 USD") 

  // 9 seeds left at 2 seeds / usd = 4.5 USD
  // 1.5 * 3 seeds = 4.5

  //9 + 4.5 = 13.5

  //11 + 13.5 = 24.5

  // cross over into a new round - 
  await contracts.exchange.newpayment(firstuser, "BTC", "02", parseInt(6 * 10000), { authorization: `${exchange}@active` })

  await check(b1, 13.5, "")

  b1 += 13.5


  await updateprice(2, 3,  55000)

  // 8.22 usd

  // 5.5 S remain at 3 s/usd
  // 5.5 .... 1.833333333 USD ==> 5.5 Seeds

  // remain: 6.3866666667

  // 10 S at 4/usd 2.5 usd 10 Seeds
  // 10 at 5 => 2 usd      10 Seeds
  // 10 at 6 => 1.666666   10 Seeds
  // 10 at 7 => 0.22 USD buys 1.54 Seeds

  // total: 37.04

  console.log("buy seeds 8.22 USD")

  await contracts.exchange.newpayment(firstuser, "BTC", "04", parseInt(8.22 * 10000), { authorization: `${exchange}@active` })

  //61.54
  await check(b1, 37.04, "")

  await updateprice(6, 7, 84600)



})


describe('Token Sale 50 Rounds', async assert => {

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
  await contracts.exchange.updatelimit("100.0000 SEEDS", "10.0000 SEEDS", "3000000.0000 SEEDS", { authorization: `${exchange}@active` })
  
  console.log(`update TLOS rate - tlos per usd`)
  await contracts.exchange.updatetlos("3.0000 SEEDS", { authorization: `${exchange}@active` })
  //await contracts.exchange.updateusd("1.0000 SEEDS", { authorization: `${exchange}@active` })

  console.log(`init token sale rounds`)

  console.log("test init rounds - 10.0000 seeds per round")
  
  let usd_per_seeds = 0.011

  var total_usd = 0

  for (let i=0; i<50; i++) {

    total_usd = total_usd + usd_per_seeds * 10

    let seeds_per_usd = 1 / usd_per_seeds

    //console.log("round "+i+" = "+seeds_per_usd.toFixed(4)+" SEEDS")

    await contracts.exchange.addround( 10 * 10000, seeds_per_usd.toFixed(4)+" SEEDS", { authorization: `${exchange}@active` })  

    usd_per_seeds = usd_per_seeds * 1.033

  }

  //console.log("rounds "+JSON.stringify(await getRounds(), null, 2))

  console.log("total value "+total_usd)

  let b1 = await getBalanceFloat(firstuser)

  const updateprice = async (expected_round, expected_price, expected_remaining) => {

    let price = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'price',
      json: true
    })
    //console.log("price: "+JSON.stringify(price, null, 2))

    assert({
      given: `new price`,
      should: `expected round: `+expected_round + " price: "+expected_price + " remaining "+expected_remaining,
      actual: price.rows[0],
      expected: {
        "id": 0,
        "current_round_id": expected_round,
        "current_seeds_per_usd": expected_price.toFixed(4)+" SEEDS",
        "remaining": expected_remaining
      }
    })

    let config = await eos.getTableRows({
      code: exchange,
      scope: exchange,
      table: 'config',
      json: true,
    })

    let seedsPerUsdString = expected_price.toFixed(4) + " SEEDS"
    
    assert({
      given: 'config table price',
      should: "price: "+expected_price,
      actual: config.rows[0].seeds_per_usd,
      expected: seedsPerUsdString
    })

  }

  const check = async (initial, expectedDelta, usd) => {
    let balance = await getBalanceFloat(firstuser)
    let delta = balance - initial
    //console.log(" expected "+expectedDelta + " actual " + delta + " initial "+ initial + " balance: "+ balance)

    assert({
      given: `purchase `+usd+" usd worth",
      should: `receive expected number of Seeds: `+expectedDelta,
      actual: delta.toFixed(4),
      expected: expectedDelta.toFixed(4)
    })
  
  }

  console.log("buy 1 seeds 0.011 USD")
  let totalbuy = 0
  let buy = 0.011
  totalbuy += buy

  await contracts.exchange.newpayment(firstuser, "BTC", "0", parseInt(buy * 10000), { authorization: `${exchange}@active` })

  let seedsBalance = 1

  await check(b1, seedsBalance, 0.011)

  b1 += seedsBalance

  await updateprice(0, 90.9091, 9 * 10000)

  buy = 0.099
  totalbuy += buy

  console.log("buy seeds USD"+buy)
  await contracts.exchange.newpayment(firstuser, "BTC", "01", parseInt(buy * 10000), { authorization: `${exchange}@active` })
  seedsBalance = 9
  await check(b1, seedsBalance, buy)
  b1 += seedsBalance
  await updateprice(1, 88.0049, 10 * 10000)


  // 13.3 seeds = 10 seeds at round 2 price of 88.0049 ==> 0.1136300365
  // 3.3 seeds are round 3 price of 85.1935 ==> 0.03873534953

  // == 0.152365386

  buy = 0.1524 // 65386
  totalbuy += buy

  console.log("buy seeds "+buy) 

  await contracts.exchange.newpayment(firstuser, "BTC", "02", parseInt(buy * 10000), { authorization: `${exchange}@active` })

  await check(b1, 13.3029, buy)

  b1 += 13.3029

  await updateprice(2, 85.1935,  (10 - 3.3029) * 10000)

  console.log("bought "+totalbuy + " out of "+total_usd) 


  console.log("buy until last round ") 
  await contracts.exchange.newpayment(firstuser, "BTC", "03999", parseInt( (total_usd - totalbuy - 0.1) * 10000), { authorization: `${exchange}@active` })
  await updateprice(49, 18.5222,  18538)

  console.log("buy in last round ") 
  await contracts.exchange.newpayment(firstuser, "BTC", "04ss", parseInt(0.001 * 10000), { authorization: `${exchange}@active` })
  await updateprice(49, 18.5222, Math.ceil(18538 - (18.5222*0.001*10000)))

  console.log("buy more than last round ") 
  let out_of_funds_purchase = false
  try {
    await contracts.exchange.newpayment(firstuser, "BTC", "05aaaa", parseInt(1 * 10000), { authorization: `${exchange}@active` })
    out_of_funds_purchase = true
  } catch (err) {
    //console.log("expected error "+err) 
  }
  assert({
    given: 'purchase more than available in rounds',
    should: 'fail',
    actual: out_of_funds_purchase,
    expected: false
  })


})
