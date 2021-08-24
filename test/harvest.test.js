const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts, createKeypair, asset, eosDevKey } = require("../scripts/helper")
const { equals } = require("ramda")
const { parse } = require("commander")
const moment = require('moment')

const { accounts, harvest, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser, proposals, organization, region, globaldho, fifthuser, escrow, pool } = names

function getBeginningOfDayInSeconds () {
  const now = new Date()
  return now.setUTCHours(0, 0, 0, 0) / 1000
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe("Harvest General", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, token, harvest, settings, history, organization })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('reset organization')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('configure')
  await contracts.settings.configure("hrvstreward", 10000 * 100, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('reset history')
  let users = [firstuser, seconduser]
  users.forEach( async (user, index) => await contracts.history.reset(user, { authorization: `${history}@active` }))

  const day = getBeginningOfDayInSeconds()

  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  await sleep(100)

  const txpoints1 = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'txpoints',
    json: true,
    limit: 100
  })

  console.log(" tx points 1 "+JSON.stringify(txpoints1, null, 2))

  
  console.log('plant seeds x')
  await contracts.token.transfer(firstuser, harvest, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })
  // await sleep(5000)

  // console.log('transfer seeds')
  await contracts.token.transfer(firstuser, seconduser, '1.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  
  // await sleep(5000)
  await contracts.token.transfer(seconduser, firstuser, '0.1000 SEEDS', '', { authorization: `${seconduser}@active` })
  
  const balanceBeforeUnplanted = await getBalanceFloat(seconduser)

  const checkTotal = async (check) => {
    const total = await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table: 'total',
      json: true,
    })
    // const planted = await eos.getTableRows({
    //   code: harvest,
    //   scope: harvest,
    //   table: 'planted',
    //   json: true,
    // })
    // console.log("planted "+JSON.stringify(planted, null, 2))
    // console.log("total "+JSON.stringify(total, null, 2))

    assert({
      given: 'checking total '+check,
      should: 'be able the same',
      actual: total.rows[0].total_planted,
      expected: check + ".0000 SEEDS"
    })
  }

  await checkTotal(700);

  let num_seeds_unplanted = 100
  await contracts.harvest.unplant(seconduser, num_seeds_unplanted + '.0000 SEEDS', { authorization: `${seconduser}@active` })

  await checkTotal(600);

  var unplantedOverdrawCheck = true
  try {
    await contracts.harvest.unplant(seconduser, '100000000.0000 SEEDS', { authorization: `${seconduser}@active` })
    unplantedOverdrawCheck = false
  } catch (err) {
    console.log("overdraw protection works")
  }

  const refundsAfterUnplanted = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })

  console.log("call first signer pays action")
  await contracts.harvest.payforcpu(
    seconduser, 
    { 
      authorization: [ 
        {
          actor: harvest,
          permission: 'payforcpu'
        },   
        {
          actor: seconduser,
          permission: 'active'
        }
      ]
    }
  )

  var payforcpuSeedsUserOnly = true
  try {
    await contracts.harvest.payforcpu(
      thirduser, 
      { 
        authorization: [ 
          {
            actor: harvest,
            permission: 'payforcpu'
          },   
          {
            actor: thirduser,
            permission: 'active'
          }
        ]
      }
    )  
    payforcpuSeedsUserOnly = false
  } catch (err) {
    console.log("require seeds user for pay for cpu test")
  }

  var payforCPURequireAuth = true
  try {
    await contracts.harvest.payforcpu(
      thirduser, 
      { 
        authorization: [    
          {
            actor: thirduser,
            permission: 'active'
          }
        ]
      }
    )  
    payforCPURequireAuth = false
  } catch (err) {
    console.log("require payforcup perm test")
  }

  var payforCPURequireUserAuth = true
  try {
    await contracts.harvest.payforcpu(
      thirduser, 
      { 
        authorization: [ 
          {
            actor: harvest,
            permission: 'payforcpu'
          },   
        ]
      }
    )  
    payforCPURequireUserAuth = false
  } catch (err) {
    console.log("require user auth test")
  }


  //console.log("results after unplanted" + JSON.stringify(refundsAfterUnplanted, null, 2))

  const balanceAfterUnplanted = await getBalanceFloat(seconduser)

  const assetIt = (string) => {
    let a = string.split(" ")
    return {
      amount: Number(a[0]) * 10000,
      symbol: a[1]
    }
  }

  const totalUnplanted = refundsAfterUnplanted.rows.reduce( (a, b) => a + assetIt(b.amount).amount, 0) / 10000

  console.log('claim refund\n')
  const balanceBeforeClaimed = await getBalanceFloat(seconduser)

  // rewind 16 days
  let day_seconds = 60 * 60 * 24
  let num_days_expired = 16
  let weeks_expired = parseInt(num_days_expired/7)
  await contracts.harvest.testclaim(seconduser, 1, day_seconds * num_days_expired, { authorization: `${harvest}@active` })

  const transactionRefund = await contracts.harvest.claimrefund(seconduser, 1, { authorization: `${seconduser}@active` })

  const refundsAfterClaimed = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })

  const balanceAfterClaimed = await getBalanceFloat(seconduser)

  //console.log(weeks_expired + " balance before claim "+balanceBeforeClaimed)
  //console.log("balance after claim  "+balanceAfterClaimed)

  let difference = balanceAfterClaimed - balanceBeforeClaimed
  let expectedDifference = num_seeds_unplanted * weeks_expired / 12

  assert({
    given: 'claim refund after '+num_days_expired+' days',
    should: 'be able to claim '+weeks_expired+'/12 of total unplanted',
    actual: Math.floor(difference * 1000)/1000,
    expected: Math.floor(expectedDifference * 1000)/1000
  })

  console.log('cancel refund')
  const transactionCancelRefund = await contracts.harvest.cancelrefund(seconduser, 1, { authorization: `${seconduser}@active` })

  const refundsAfterCanceled = await getTableRows({
    code: harvest,
    scope: seconduser,
    table: 'refunds',
    json: true,
    limit: 100
  })

  console.log('sow seeds')
  await contracts.harvest.sow(seconduser, firstuser, '10.0000 SEEDS', { authorization: `${seconduser}@active` })

  var sowBalanceExceeded = false
  try {
    await contracts.harvest.sow(seconduser, firstuser, '100000000000.0000 SEEDS', { authorization: `${seconduser}@active` })
    sowBalanceExceeded = true
  } catch (err) {
    console.log("trying to sow more than user has planted throws error")
  }

  console.log('calculate planted score')
  await contracts.harvest.rankplanteds({ authorization: `${harvest}@active` })
  const planted = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'planted',
    json: true
  })
  const sizes = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'sizes',
    json: true
  })
  console.log("plantedXX  "+JSON.stringify(planted, null, 2))
  console.log("sizesxx  "+JSON.stringify(sizes, null, 2))


  console.log('calculate reputation multiplier')
  await contracts.accounts.addrep(firstuser, 1, { authorization: `${accounts}@active` })
  await contracts.accounts.addrep(seconduser, 2, { authorization: `${accounts}@active` })
  await contracts.accounts.rankreps({ authorization: `${accounts}@active` })

  await contracts.token.transfer(firstuser, seconduser, '1.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await sleep(2000)
  
  await contracts.token.transfer(seconduser, firstuser, '0.1000 SEEDS', '', { authorization: `${seconduser}@active` })
  await sleep(2000)

  console.log('calculate transactions score')
  await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
  await contracts.harvest.ranktxs({ authorization: `${harvest}@active` })

  const txpoints = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'txpoints',
    json: true
  })


  console.log('organizations planted fee')
  const eosDevKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'
  const org1 = 'org1'

  await contracts.token.transfer(firstuser, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.organization.create(firstuser, org1, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, org1, "200.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  
  await contracts.token.transfer(org1, harvest, "200.0000 SEEDS", "sow " + org1, { authorization: `${org1}@active` })

  let canNotUnplantOrgFee = true
  try {
    await contracts.harvest.unplant(org1, '200.0001 SEEDS', { authorization: `${org1}@active` })
    canNotUnplantOrgFee = false
  } catch (error) {
    console.log('can not unplant org fee (expected)')
  }

  await contracts.harvest.unplant(org1, '200.0000 SEEDS', { authorization: `${org1}@active` })

  const plantedOrgs = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'planted',
    json: true
  })

  let transactionAsString = JSON.stringify(transactionRefund.processed)

  console.log("includes history "+transactionAsString.includes(history))
  console.log("includes history "+transactionAsString.includes("trxentry"))

  assert({
    given: 'sow more than planted',
    should: 'throw exception',
    actual: sowBalanceExceeded,
    expected: false
  })

  assert({
    given: 'claim refund transaction',
    should: 'call inline action to history',
    actual: transactionAsString.includes(history) && transactionAsString.includes("trxentry") ,
    expected: true
  })

  assert({
    given: 'unplant more than planted',
    should: 'fail',
    actual: unplantedOverdrawCheck,
    expected: true
  })
  
  assert({
    given: 'cancel refund transaction',
    should: 'call inline action to history',
    actual: transactionCancelRefund.processed.action_traces[0].inline_traces[0].act.account,
    expected: history
  })

  assert({
    given: 'after unplanting 100 seeds',
    should: 'refund rows add up to 100',
    actual: totalUnplanted,
    expected: 100
  })

  assert({
    given: 'unplant called',
    should: 'create refunds rows',
    actual: refundsAfterUnplanted.rows.length,
    expected: 12
  })

  assert({
    given: 'claimed refund',
    should: 'keep refunds rows',
    actual: refundsAfterClaimed.rows.length,
    expected: 10
  })

  assert({
    given: 'canceled refund',
    should: 'withdraw refunds to user',
    actual: refundsAfterCanceled.rows.length,
    expected: 0
  })

  assert({
    given: 'unplanting process',
    should: 'not change user balance before timeout',
    actual: balanceAfterUnplanted,
    expected: balanceBeforeUnplanted
  })

  assert({
    given: 'planted calculation',
    should: 'assign planted score to each user',
    actual: planted.rows.map(row => ({
      account: row.account,
      score: row.rank
    })),
    expected: [{
      account: firstuser,
      score: 27
    }, {
      account: seconduser,
      score: 0
    }]
  })

  console.log("txpoints 77: "+JSON.stringify(txpoints, null, 2))
  
  assert({
    given: 'transactions calculation',
    should: 'assign transactions score to each user',
    actual: txpoints.rows.map(({ rank }) => rank).sort((a, b) => b - a),
    expected: [0]
  })

  assert({
    given: 'payforcpu called',
    should: 'work only when both authorizations are provided and user is seeds user',
    actual: [payforCPURequireAuth, payforCPURequireUserAuth, payforcpuSeedsUserOnly],
    expected: [true, true, true]
  })

  assert({
    given: 'organization tried to unplant some of the org fee',
    should: 'not allow that',
    actual: canNotUnplantOrgFee,
    expected: true
  })

  assert({
    given: 'organization unplanted',
    should: 'unplant',
    actual: plantedOrgs.rows.filter(p => p.account == org1)[0].planted,
    expected: '200.0000 SEEDS'
  })

})

describe("harvest planted score", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, token, harvest, settings })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  await contracts.harvest.rankplanteds({ authorization: `${harvest}@active` })
  await contracts.accounts.rankreps({ authorization: `${accounts}@active` })
  await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
  await contracts.harvest.ranktxs({ authorization: `${harvest}@active` })

  const planted = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'planted',
    json: true,
    limit: 100
  })

  console.log('planted '+JSON.stringify(planted, null, 2))

  assert({
    given: 'planted calculation',
    should: 'have values',
    actual: planted.rows.map(({ rank }) => rank),
    expected: [27, 0]
  })

})

describe("harvest transaction score", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const memoprefix = "" + (new Date()).getTime()

  const contracts = await initContracts({ accounts, token, harvest, settings, history })

  const day = getBeginningOfDayInSeconds()

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('change max limit transactions')
  await contracts.settings.configure('txlimit.min', 100, { authorization: `${settings}@active` })
  await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

  console.log('join users')
  let users = [firstuser, seconduser, thirduser, fourthuser]
  users.forEach( async (user, index) => await contracts.accounts.adduser(user, index+' user', 'individual', { authorization: `${accounts}@active` }))
  users.forEach( async (user, index) => await contracts.history.reset(user, { authorization: `${history}@active` }))
  
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  await sleep(100)

  const transfer = async (from, to, quantity, memo) => {
    await contracts.token.transfer(from, to, `${quantity}.0000 SEEDS`, memo, { authorization: `${from}@active` })
    await sleep(3000)
  }

  const checkScores = async (points, scores, given, should) => {

    console.log("checking points " + points + " scores: " + scores)
    await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
    await contracts.harvest.ranktxs({ authorization: `${harvest}@active` })
    
    const txpoints = await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table: 'txpoints',
      json: true,
      limit: 100
    })
    console.log(" tx points "+JSON.stringify(txpoints, null, 2))
    
    assert({
      given: 'transaction points ' + given,
      should: 'have expected values ' + should,
      actual: txpoints.rows.map(({ points }) => points),
      expected: points
    })

    assert({
      given: 'transaction scores ' + given,
      should: 'have expected values ' + should,
      actual: txpoints.rows.map(({ rank }) => rank),
      expected: scores
    })

  }

  //await contracts.accounts.rankcbss({ authorization: `${accounts}@active` })

  // await checkScores([], [], "no reputation no points", "be empty")

  console.log('calculate tx scores with reputation')
  await contracts.accounts.testsetrs(seconduser, 49, { authorization: `${accounts}@active` })

  console.log('make transaction, no reps')
  await transfer(firstuser, seconduser, 10, memoprefix)

  await checkScores([10], [0], "1 reputation, 1 tx", "100 score")

  // console.log("transfer with 10 rep, 2 accounts have rep")
  await contracts.accounts.testsetrs(thirduser, 75, { authorization: `${accounts}@active` })
  await transfer(seconduser, thirduser, 10, '0'+memoprefix)

  await checkScores([10, 16], [0, 27], "2 reputation, 2 tx", "0, 27 score")

  let expectedScore = 15 + 25 * (1 * 1.5) // 52.5
  console.log("More than 26 transactions. Expected tx points: "+ expectedScore)
  for(let i = 0; i < 40; i++) {
    // 40 transactions
    // rep multiplier 2nd user: 1.5
    // vulume: 1
    // only 26 tx count
    // score from before was 15
    await transfer(firstuser, seconduser, 9, memoprefix+" tx "+i)
  }
  await checkScores([19, 16], [27, 0], "2 reputation, 2 tx", "27, 0 score")

  // test tx exceeds volume limit
  let tx_max_points = 1777
  let third_user_rep_multiplier = 2 * 0.7575
  await transfer(seconduser, thirduser, 3000, memoprefix+" tx max pt")
  await checkScores([19, parseInt(Math.ceil(16 + tx_max_points * third_user_rep_multiplier))], [0, 27], "large tx", "100, 27 score")
  
  // send back 
  await transfer(thirduser, seconduser, 3000, memoprefix+" tx max pt")

  console.log("calc CS score")
  await contracts.harvest.calccss({ authorization: `${harvest}@active` })
  await contracts.harvest.rankcss({ authorization: `${harvest}@active` })

  const cspoints = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'cspoints',
    json: true,
    limit: 100
  })

  // let secondCSpoints = cspoints.rows.filter( item => item.account == seconduser )[0].contribution_points
  // let secondCS = harvestStats.rows.filter( item => item.account == seconduser )[0].contribution_score

  const txpoints = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'txpoints',
    json: true,
    limit: 100
  })
  console.log(" tx points "+JSON.stringify(txpoints, null, 2))
  

  console.log("cs points "+JSON.stringify(cspoints, null, 2))

  assert({
    given: 'contribution score points',
    should: 'have contribution points',
    actual: cspoints.rows.map(({ contribution_points }) => contribution_points), 
    expected: [26]
  })

  assert({
    given: 'contribution score',
    should: 'have contribution score score',
    actual: cspoints.rows.map(({ rank }) => rank), 
    expected: [0]
  })

})


describe("harvest community building score", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  const contracts = await initContracts({ accounts, harvest, settings, history })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('join users')
  let users = [firstuser, seconduser, thirduser, fourthuser]

  for (let i = 0; i < users.length; i++) {
    await contracts.accounts.adduser(users[i], i+' user', 'individual', { authorization: `${accounts}@active` })
    await sleep(400)
  }

  const checkScores = async (points, scores, given, should) => {

    console.log("checking points "+points + " scores: "+scores)
    await sleep(300)
    await contracts.accounts.rankcbss({ authorization: `${accounts}@active` })
    
    const cbs = await eos.getTableRows({
      code: accounts,
      scope: accounts,
      table: 'cbs',
      json: true,
      limit: 100
    })

    //console.log(given + " cba points "+JSON.stringify(cbs, null, 2))
  
    assert({
      given: 'cbs points '+given,
      should: 'have expected values '+should,
      actual: cbs.rows.map(({ community_building_score }) => community_building_score),
      expected: points
    })

    assert({
      given: 'cbs scores '+given,
      should: 'have expected values '+should,
      actual: cbs.rows.map(({ rank }) => rank),
      expected: scores
    })

  }

  console.log('add cbs')
  //await contracts.accounts.rankcbss({ authorization: `${accounts}@active` })

  //await checkScores([], [], "no cbs", "be empty")

  console.log('calculate cbs scores')
  await contracts.accounts.testsetcbs(firstuser, 1, { authorization: `${accounts}@active` })
  await sleep(200)
  await checkScores([1], [0], "1 cbs", "0 score")

  await contracts.accounts.testsetcbs(seconduser, 2, { authorization: `${accounts}@active` })
  await sleep(200)
  await contracts.accounts.testsetcbs(thirduser, 3, { authorization: `${accounts}@active` })
  await sleep(200)
  await contracts.accounts.testsetcbs(fourthuser, 0, { authorization: `${accounts}@active` })

  await contracts.accounts.rankcbss({ authorization: `${accounts}@active` })
  await checkScores([1, 2, 3, 0], [4, 27, 63, 0], "cbs distribution", "correct")
})

describe('contribution score', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  const eosDevKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
    eos.contract(organization),
    eos.contract(history)
  ]).then(([token, accounts, harvest, settings, organization, history]) => ({
    token, accounts, harvest, settings, organization, history
  }))

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('harvest reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset organization')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('reset history')
  const day = getBeginningOfDayInSeconds()
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.reset(fifthuser, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

  const users = [firstuser, seconduser, thirduser, fourthuser]

  const org1 = 'testorg1'
  const org2 = 'testorg2'
  const org3 = 'testorg3'

  const orgs = [org1, org2, org3]

  await Promise.all(users.map(user => contracts.history.reset(user, { authorization: `${history}@active` })))
  await Promise.all(orgs.map(org => contracts.history.reset(org, { authorization: `${history}@active` })))

  const checkCSScores = async (scope, scores, rankings) => {
    const rankcss = await eos.getTableRows({
      code: harvest,
      scope,
      table: 'cspoints',
      json: true,
      limit: 100
    })
    console.log(rankcss)
    assert({
      given: 'contribution score',
      should: 'have the correct values',
      actual: {
        scores: rankcss.rows.map(r => r.contribution_points),
        rankings: rankcss.rows.map(r => r.rank)
      },
      expected: {
        scores,
        rankings
      }
    })
  }

  const getEntry = (user, rows) => {
    const aux = rows.filter(r => r.account == user)
    if (aux.length > 0) {
      return aux[0].rank
    }
    return 0
  }

  const calcCSPoints = async (user, s) => {
    const plant = await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table: 'planted',
      json: true,
      limit: 100
    })
    const cbs = await eos.getTableRows({
      code: accounts,
      scope: s == 'org' ? s : accounts,
      table: 'cbs',
      json: true,
      limit: 100
    })
    const trx = await eos.getTableRows({
      code: harvest,
      scope: s == 'org' ? s : harvest,
      table: 'txpoints',
      json: true,
      limit: 100
    })
    const rep = await eos.getTableRows({
      code: accounts,
      scope: s == 'org' ? s : accounts,
      table: 'rep',
      json: true,
      limit: 100
    })
    const plantedRank = getEntry(user, plant.rows)
    const cbsRank = getEntry(user, cbs.rows)
    const trxRank = getEntry(user, trx.rows)
    const repRank = getEntry(user, rep.rows)

    return parseInt(((plantedRank + cbsRank + trxRank) * repRank * 2) / 100)
  }

  const individualHarvestScope = harvest
  const organizationScope = 'org'

  console.log('join users')
  
  await contracts.accounts.adduser(fifthuser, fifthuser, 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testsetrs(fifthuser, 10, { authorization: `${accounts}@active` })

  for (let i = 0; i < users.length; i++) {
    const user = users[i]
    await contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(user, 10*(i+1), { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(user, i+1, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetcbs(user, 10*(i+1), { authorization: `${accounts}@active` })
    await contracts.token.transfer(user, harvest, `${10 * (i+1)}.0000 SEEDS`, `sow ${user}`, { authorization: `${user}@active` })
    await contracts.token.transfer(user, fifthuser, `${50 * (i+1)}.0000 SEEDS`, '', { authorization: `${user}@active` })
    await contracts.history.reset(user, { authorization: `${history}@active` })
  }
  
  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

  console.log('create organization')
  
  await contracts.organization.create(firstuser, 'testorg1', "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
  await contracts.organization.create(firstuser, 'testorg2', "Org 2", eosDevKey,  { authorization: `${firstuser}@active` })
  await contracts.organization.create(seconduser, 'testorg3', "Org 3 - Test, Inc.", eosDevKey, { authorization: `${seconduser}@active` })

  for (let i = 1; i <= orgs.length; i++) {
    const org = orgs[i-1]
    await contracts.accounts.addrep(org, 20*i, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(org, i, { authorization: `${accounts}@active` })
    await contracts.token.transfer(firstuser, org, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.accounts.testsetcbs(org, 20*i, { authorization: `${accounts}@active` })
    await contracts.token.transfer(org, fifthuser, `${100 * (i+1)}.0000 SEEDS`, '', { authorization: `${org}@active` })
  }

  await sleep(2000)

  console.log('rank cbs')
  await contracts.accounts.rankcbss({ authorization: `${accounts}@active` })
  await contracts.accounts.rankorgcbss({ authorization: `${accounts}@active` })
  await sleep(1000)

  console.log('rank rep')
  await contracts.accounts.rankreps({ authorization: `${accounts}@active` })
  await contracts.accounts.rankorgreps({ authorization: `${accounts}@active` })
  await sleep(1000)

  console.log('rank transactions')
  await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
  await sleep(1000)
  await contracts.harvest.ranktxs({ authorization: `${harvest}@active` })
  await contracts.harvest.rankorgtxs({ authorization: `${harvest}@active` })
  await sleep(1000)
  
  console.log('rank planted')
  await contracts.harvest.rankplanteds({ authorization: `${harvest}@active` })
  await sleep(1000)
  
  console.log('calculate contribution score for orgs')
  await contracts.harvest.calccss({ authorization: `${harvest}@active` })
  await sleep(2000)

  let userScores = []
  for (const user of users) { userScores.push(await calcCSPoints(user, accounts)) }
  userScores = userScores.filter(s => s > 0)

  let orgScores = []
  for (const org of orgs) { orgScores.push(await calcCSPoints(org, 'org')) }
  orgScores = orgScores.filter(s => s > 0)

  await checkCSScores(individualHarvestScope, userScores, [0, 0, 0, 0])
  await checkCSScores(organizationScope, orgScores, [0, 0])

  console.log('rank contribution score for orgs')
  await contracts.harvest.rankcss({ authorization: `${harvest}@active` })
  await contracts.harvest.rankorgcss({ authorization: `${harvest}@active` })
  await sleep(2000)

  await checkCSScores(individualHarvestScope, userScores, [25, 0, 50, 75])
  await checkCSScores(organizationScope, orgScores, [0, 50])

})

describe("plant for other user", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
  ]).then(([token, accounts, harvest, settings]) => ({
    token, accounts, harvest, settings
  }))

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  let memo = "sow "+seconduser

  console.log('plant seeds with memo: ' + memo)

  await contracts.token.transfer(firstuser, harvest, '77.0000 SEEDS', memo, { authorization: `${firstuser}@active` })

  const plantedBalances = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'balances',
    upper_bound: seconduser,
    lower_bound: seconduser,
    json: true,
  })

  let badMemos = false
  const badMemo = async (badmemo) => {
    try {
      await contracts.token.transfer(firstuser, harvest, '77.0000 SEEDS', badmemo, { authorization: `${firstuser}@active` })
      badMemos = true
    } catch (error) {
      //console.log("error as expected "+ badmemo)
      //console.log(error)
    }  
  }
  //console.log('planted: ' + JSON.stringify(plantedBalances, null, 2))

  await badMemo("foobar");
  await badMemo("ASDASDSDASDSADSDSDSDASDSDSDSSDSDSDSADSDSADSD");
  await badMemo("sow X");
  await badMemo("sow somethingspecial");
  await badMemo("sow seedsuserfoo");
  await badMemo("sow seedsuseraaa foo");

  await badMemo("sow .");
  await badMemo("sow \u03A9\u0122\u9099\u6660");
  await badMemo("sow sadsadsakd;skdjlksajdlaskjd;lkaslaksdj;lkasjdals;kdjsal;kdj;aslKDJ;alskdj;alskdj;alskdj;alskdj;alskdjas;lKDJas;lkdj;alsKDJ;aslkdja;sLKDJas;lkdj");

  assert({
    given: 'user '+firstuser + 'planted for '+seconduser,
    should: 'second user should have planted balance',
    actual: plantedBalances.rows[0],
    expected: {
      "account": seconduser,
      "planted": "77.0000 SEEDS",
      "reward": "0.0000 SEEDS"
    }
  })
  assert({
    given: 'bad memo',
    should: 'cause exception',
    actual: badMemos,
    expected: false
  })

})


describe('Monthly QEV', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
    eos.contract(history)
  ]).then(([token, accounts, harvest, settings, history]) => ({
    token, accounts, harvest, settings, history
  }))

  const day = getBeginningOfDayInSeconds()

  console.log('reset history')
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('update circulaing supply')
  await contracts.token.updatecirc({ authorization: `${token}@active` })

  console.log('calculate total monthly qev')
  
  await contracts.history.testtotalqev(120, 100 * 10000, { authorization: `${history}@active` })
  await sleep(100)
  await contracts.harvest.calcmqevs({ authorization: `${harvest}@active` })

  const totalQev = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'monthlyqevs',
    json: true,
  })

  delete totalQev.rows[0].circulating_supply

  assert({
    given: 'monthly qev calculated',
    should: 'have the correct monthlyqevs entries',
    actual: totalQev.rows,
    expected: [
      { timestamp: day, qualifying_volume: 30000000 }
    ]
  })

})

async function testHarvest (assert, dSeeds) {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  let eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
    eos.contract(history),
    eos.contract(organization),
    eos.contract(region)
  ]).then(([token, accounts, harvest, settings, history, organization, region]) => ({
    token, accounts, harvest, settings, history, organization, region
  }))

  const day = getBeginningOfDayInSeconds()
  const secondsPerDay =  86400
  const moonCycle = secondsPerDay * 29 + parseInt(secondsPerDay / 2)
  const previousDay = (new Date((day - 3 * moonCycle) * 1000).setUTCHours(0,0,0,0)) / 1000

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const orgs = ['orgaaa', 'orgbbb', 'orgccc', 'orgddd']

  const minEligibleOrgStatus = 2

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('configure - rgn.cit')
  await contracts.settings.configure("rgn.cit", 1, { authorization: `${settings}@active` })

  console.log('reset history')
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  for (const user of users) {
    await contracts.history.reset(user, { authorization: `${history}@active` })
  }
  for (const org of orgs) {
    await contracts.history.reset(org, { authorization: `${history}@active` })
  }
  
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset orgs')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('reset rgns')
  await contracts.region.reset({ authorization: `${region}@active` })

  console.log('reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('configure percentages')
  const percentageForUsers = 0.3
  const percentageForOrgs = 0.2
  const percentageForrgns = 0.3
  const percentageForGlobal = 0.2

  await contracts.settings.configure('hrvst.users', parseInt(percentageForUsers*1000000), { authorization: `${settings}@active` })
  await contracts.settings.configure('hrvst.orgs', parseInt(percentageForOrgs*1000000), { authorization: `${settings}@active` })
  await contracts.settings.configure('hrvst.rgns', parseInt(percentageForrgns*1000000), { authorization: `${settings}@active` })
  await contracts.settings.configure('hrvst.global', parseInt(percentageForGlobal*1000000), { authorization: `${settings}@active` })


  console.log('update circulaing supply')
  await contracts.token.updatecirc({ authorization: `${token}@active` })

  console.log('calculate total monthly qev')
  await contracts.history.testtotalqev(120, 100 * 10000, { authorization: `${history}@active` })
  await sleep(100)
  await contracts.harvest.calcmqevs({ authorization: `${harvest}@active` })

  const getTestBalance = async (user) => {
    const balance = await eos.getCurrencyBalance(names.token, user, 'TESTS')
    return Number.parseFloat(balance[0]) || 0
  }

  const getHarvestBalance = async (rgn, scope='test') => {
    const hbalances = await getTableRows({
      code: region,
      scope: scope,
      table: 'hrvstrgnblnc',
      json: true
    })
    let value = 0.0
    const hbalance = hbalances.rows.filter(r => r.region == rgn)
    if (hbalance.length > 0) {
      value = parseFloat(hbalance[0].balance.split(' ')[0])
    }
    return value
  }

  const checkHarvestValues = (bucket, ranks, totalAmount, actualValues) => {

    let totalRank

    if (bucket !== 'orgs') {
      totalRank = ranks.reduce((acc, curr) => acc + curr)
    } else {
      totalRank = 0
      for (const r of ranks) {
        totalRank += r.status >= minEligibleOrgStatus ? r.rank : 0
      }
    }

    const fragmentSeeds = totalAmount / totalRank

    const expectedSeeds = ranks.map((rank) => {
      let r = 0
      if (bucket === 'orgs') {
        if (rank.status >= minEligibleOrgStatus) {
          r = rank.rank
        }
      } else { 
        r = rank
      }
      const temp = r * fragmentSeeds
      return parseFloat(temp.toFixed(4))
    })

    const adjustedActualValues = actualValues.map(seeds => parseFloat(seeds.toFixed(4)))
    const assertBalances = adjustedActualValues.map((balance, index) => {
      if (isNaN(balance)) {
        if (isNaN(expectedSeeds[index]) || expectedSeeds[index] == 0) {
          return true
        }
        return false
      }
      if (Math.abs(balance - expectedSeeds[index]) <= 0.0002) { return true }
      return false
    })

    console.log('expected:', expectedSeeds)
    console.log('actual:', adjustedActualValues)

    assert({
      given: `harvest distributed for ${bucket}`,
      should: 'have the correct balances',
      expected: new Array(ranks.length).fill(true),
      actual: assertBalances
    })
    
  }

  console.log('add users')
  for (let index = 0; index < users.length; index++) {
    const user = users[index]
    await contracts.accounts.adduser(user, index+' user', 'individual', { authorization: `${accounts}@active` })
    await contracts.harvest.testcspoints(user, (index+1) * 33, { authorization: `${harvest}@active` })
  }
  await sleep(100)

  console.log('add orgs')
  for (let index = 0; index < orgs.length; index++) {
    const org = orgs[index]
    await contracts.token.transfer(firstuser, organization, '200.0000 SEEDS', 'initial supply', { authorization: `${firstuser}@active` })
    await contracts.organization.create(firstuser, org, `${org} name`, eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.teststatus(org, index, { authorization: `${organization}@active` })
    await contracts.harvest.testcspoints(org, (index+1) * 50, { authorization: `${harvest}@active` })
  }

  console.log('add regions')
  const keypair = await createKeypair();
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })
  const rgns = ['rgn1.rgn', 'rgn2.rgn']
  for (let index = 0; index < rgns.length; index++) {
    const rgn = rgns[index]
    await contracts.token.transfer(users[index], region, "1.0000 SEEDS", "Initial supply", { authorization: `${users[index]}@active` })
    await contracts.region.create(
      users[index], 
      rgn, 
      'test rgn region',
      '{lat:0.0111,lon:1.3232}', 
      1.1, 
      1.23,  
      { authorization: `${users[index]}@active` })
  }

  await contracts.harvest.rankcss({ authorization: `${harvest}@active` })
  await contracts.harvest.rankorgcss({ authorization: `${harvest}@active` })
  await sleep(2000)

  // ----------------------------------------- //
  const csTable = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'cspoints',
    json: true,
  })
  console.log(csTable)

  const csOrgTable = await getTableRows({
    code: harvest,
    scope: 'org',
    table: 'cspoints',
    json: true,
  })
  console.log(csOrgTable)

  const regions = await getTableRows({
    code: region,
    scope: region,
    table: 'regions',
    json: true,
  })
  //console.log(regions)
  // ----------------------------------------- //


  const mqevsBefore = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'monthlyqevs',
    json: true,
  })
  console.log(mqevsBefore)

  const currentCirculatingSupply = mqevsBefore.rows[0].circulating_supply
  const pastCirculatingSupply = currentCirculatingSupply - 5000 * 10000
  const expectedVolumeGrowth = 0.2
  const targetSupply = (1 + expectedVolumeGrowth) * pastCirculatingSupply
  const delta = targetSupply - currentCirculatingSupply
  const expectedMintRate = parseInt(delta / 708)

  await contracts.harvest.testcalcmqev(previousDay, 2500 * 10000, pastCirculatingSupply, { authorization: `${harvest}@active` })

  await contracts.harvest.calcmintrate({ authorization: `${harvest}@active` })

  await contracts.token.updatecirc({ authorization: `${token}@active` })

  const mintRateTable = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'mintrate',
    json: true,
  })
  delete mintRateTable.rows[0].timestamp

  let poolPayout = 0
  const mintRate = mintRateTable.rows[0].mint_rate / 10000.0
  console.log('mint rate:', mintRate)

  if (dSeeds > 0) {
    poolPayout = Math.min(mintRate * 0.5, dSeeds)
    console.log('pool payout:', poolPayout)
  }

  const mintedSeeds = mintRate - poolPayout
  console.log('minted seeds:', mintedSeeds)
  
  const userBalancesBefore = await Promise.all(users.map(user => getTestBalance(user)))
  const orgBalancesBefore = await Promise.all(orgs.map(org => getTestBalance(org)))
  const rgnBalancesBefore = await Promise.all(rgns.map(rgn => getHarvestBalance(rgn)))
  const globalBalanceBefore = await getTestBalance(globaldho)

  console.log('run harvest')
  await contracts.harvest.runharvest({ authorization: `${harvest}@active` })
  console.log('harvest done')

  await sleep(1000)

  const userBalancesAfter = await Promise.all(users.map(user => getTestBalance(user)))
  const orgBalancesAfter = await Promise.all(orgs.map(org => getTestBalance(org)))
  const rgnBalancesAfter = await Promise.all(rgns.map(rgn => getHarvestBalance(rgn)))
  const globalBalanceAfter = await getTestBalance(globaldho)

  const userHarvest = userBalancesAfter.map((seeds, index) => seeds - userBalancesBefore[index])
  const orgsHarvest = orgBalancesAfter.map((seeds, index) => seeds - orgBalancesBefore[index])
  const rgnsHarvest = rgnBalancesAfter.map((seeds, index) => seeds - rgnBalancesBefore[index])
  const globalHarvest = globalBalanceAfter - globalBalanceBefore

  console.log('users:', userHarvest)
  console.log('orgs:', orgsHarvest)
  console.log('rgns:', rgnsHarvest)
  console.log('global:', globalHarvest)

  const organizationsTable = await getTableRows({
    code: organization,
    scope: organization,
    table: 'organization',
    json: true,
  })
  const orgStatus = {}
  for (const r of organizationsTable.rows) {
    orgStatus[r.org_name] = r.status
  }

  console.log('check expected values')
  checkHarvestValues('users', csTable.rows.filter(row => users.includes(row.account)).map(row => row.rank), mintedSeeds * percentageForUsers, userHarvest)
  checkHarvestValues('orgs', csOrgTable.rows.map(row => { return { rank:row.rank, status: orgStatus[row.account] } }), mintedSeeds * percentageForOrgs, orgsHarvest)
  checkHarvestValues('rgns', new Array(rgns.length).fill(1), mintedSeeds * percentageForrgns, rgnsHarvest)
  checkHarvestValues('global', [1], mintedSeeds * percentageForGlobal, [globalHarvest])

  const stats = await getTableRows({
    code: token,
    scope: 'TESTS',
    table: 'stat',
    json: true
  })
  console.log('stats', stats)

  assert({
    given: 'mint rate calculated',
    should: 'have the correct values',
    actual: mintRateTable.rows,
    expected: [{
      id: 0,
      mint_rate: expectedMintRate,
      volume_growth: parseInt(expectedVolumeGrowth * 10000)
    }]
  })

  const harvestBalances = await getTableRows({
    code: region,
    scope: 'test',
    table: 'hrvstrgnblnc',
    json: true
  })
  console.log('harvestBalances:', harvestBalances)

  return mintedSeeds

}

describe('Mint Rate and Harvest', async assert => {

  const contracts = await initContracts({ pool })

  console.log('pool reset')
  await contracts.pool.reset({ authorization: `${pool}@active` })

  await testHarvest(assert, 0)
})

describe('Mint Rate and Harvest, dSeeds > 0', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, pool, token, escrow, settings })

  const hyphadao = 'dao.hypha'
  const golive = 'golive'
  const users = [firstuser, seconduser, thirduser, fourthuser]

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('pool reset')
  await contracts.pool.reset({ authorization: `${pool}@active` })

  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
  
  console.log('create locks')
  await contracts.token.transfer(firstuser, escrow, '60000.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  const vesting_date_future = moment().utc().add(1000, 's').toString()
  await Promise.all(
      users
      .slice(1)
      .map((user, index) => 
          contracts.escrow.lock(
              'event', 
              firstuser, 
              user, 
              `${10000 * (index+1)}.0000 SEEDS`, 
              golive, 
              hyphadao, 
              vesting_date_future, 
              'notes', 
              { authorization: `${firstuser}@active` }
          )
      )
  )

  const escrowT = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })
  console.log(escrowT)

  console.log('trigger event golive')
  await contracts.escrow.resettrigger(hyphadao, { authorization: `${escrow}@active` })
  await contracts.escrow.triggertest(hyphadao, golive, 'event notes', { authorization: `${escrow}@active` })
  await sleep(2000)

  const mintedSeeds = await testHarvest(assert, 60000)

  const poolBalanceTable = await getTableRows({
    code: pool,
    scope: pool,
    table: 'balances',
    json: true
  })
  console.log(poolBalanceTable)

  const expectedBalances = users.slice(1).map((user, index) => {
    const userBalance = 10000 * (index + 1)
    return userBalance - (mintedSeeds * (userBalance / 60000))
  })
  const actual = poolBalanceTable.rows.map((r, index) => {
    const actualBalance = asset(r.balance).amount
    return Math.abs(actualBalance - expectedBalances[index]) <= 0.0001
  })

  assert({
    given: 'harvest run',
    should: 'repart the dSeeds',
    actual,
    expected: Array.from(Array(expectedBalances.length).keys()).fill(true)
  })

})

describe('regions contribution score', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  let eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

  const contracts = await Promise.all([
    eos.contract(token),
    eos.contract(accounts),
    eos.contract(harvest),
    eos.contract(settings),
    eos.contract(history),
    eos.contract(organization),
    eos.contract(region)
  ]).then(([token, accounts, harvest, settings, history, organization, region]) => ({
    token, accounts, harvest, settings, history, organization, region
  }))

  const day = getBeginningOfDayInSeconds()
  console.log('reset history')
  await contracts.history.reset(history, { authorization: `${history}@active` })
  await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset rgns')
  await contracts.region.reset({ authorization: `${region}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  // console.log('configure - rgn.cit')
  // await contracts.settings.configure("rgn.cit", 2, { authorization: `${settings}@active` })

  console.log('join users')
  const users = [firstuser, seconduser, thirduser, fourthuser, fifthuser]
  for (let i = 0; i < users.length; i++) {
    const user = users[i]
    await contracts.accounts.adduser(user, i + ' user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(user, 49, { authorization: `${accounts}@active` })
    await contracts.history.reset(user, { authorization: `${history}@active` })
  }

  console.log('add regions')
  const keypair = await createKeypair();
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })
  const rgns = ['rgn1.rgn', 'rgn2.rgn', 'rgn3.rgn']
  for (let index = 0; index < rgns.length; index++) {
    const rgn = rgns[index]
    await contracts.token.transfer(users[index], region, "1.0000 SEEDS", "Initial supply", { authorization: `${users[index]}@active` })
    await contracts.region.create(
      users[index], 
      rgn, 
      'test rgn region',
      '{lat:0.0111,lon:1.3232}', 
      1.1, 
      1.23, 
      { authorization: `${users[index]}@active` })
  }

  await contracts.region.join('rgn2.rgn', fourthuser,{ authorization: `${fourthuser}@active` })
  await contracts.region.join('rgn3.rgn', fifthuser,{ authorization: `${fifthuser}@active` })

  console.log('transfer')
  for (let i = 0; i < users.length; i++) {
    const user = users[i]
    if (i === 0) {
      await contracts.token.transfer(user, seconduser, '1000.0000 SEEDS', 'supply', { authorization: `${user}@active` })
    } else {
      await contracts.token.transfer(user, firstuser, '1000.0000 SEEDS', 'supply', { authorization: `${user}@active` })
    }
    await sleep(2000)
  }

  console.log('rank transactions')
  await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
  await contracts.harvest.ranktxs({ authorization: `${harvest}@active` })

  console.log('calc contribution score')
  await contracts.harvest.calccss({ authorization: `${harvest}@active` })

  console.log('change max limit transactions')
  await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

  console.log('calc contribution score')
  await contracts.harvest.rankrgncss({ authorization: `${harvest}@active` })
  await sleep(5000)

  const cspointsrgns = await getTableRows({
    code: harvest,
    scope: 'rgn',
    table: 'cspoints',
    json: true
  })

  const cspointsrgnsTemp = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'regioncstemp',
    json: true
  })

  assert({
    given: 'cs for regions',
    should: 'have the correct ranks',
    actual: cspointsrgns.rows,
    expected: [
      { account: 'rgn2.rgn', contribution_points: 41, rank: 0 },
      { account: 'rgn3.rgn', contribution_points: 82, rank: 50 }
    ]
  })

  assert({
    given: 'cs for regions, the table regioncstemp',
    should: 'not have entries',
    actual: cspointsrgnsTemp.rows,
    expected: []
  })

})


