const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, harvest, token, firstuser, seconduser, thirduser, bank, settings, history } = names

describe("harvest", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

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

  console.log('configure')
  await contracts.settings.configure("hrvstreward", 10000 * 100, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  await contracts.token.transfer(firstuser, harvest, '999.0000 CRRTS', '', { authorization: `${firstuser}@active` })

  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  const plantedBalances = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'balances',
    json: true,
    limit: 100
  })

  const balanceBeforeUnplanted = await getBalanceFloat(seconduser)

  let num_seeds_unplanted = 100
  await contracts.harvest.unplant(seconduser, num_seeds_unplanted + '.0000 SEEDS', { authorization: `${seconduser}@active` })

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
  await contracts.harvest.calcplanted({ authorization: `${harvest}@active` })

  console.log('calculate transactions score')
  await contracts.harvest.calctrx({ authorization: `${harvest}@active` })

  console.log('calculate reputation multiplier')
  await contracts.accounts.addrep(firstuser, 1, { authorization: `${accounts}@active` })
  await contracts.accounts.addrep(seconduser, 2, { authorization: `${accounts}@active` })
  await contracts.harvest.calcrep({ authorization: `${harvest}@active` })

  console.log('claim reward')
  await contracts.harvest.testreward(seconduser, { authorization: `${harvest}@active` })


  var balanceBefore = await getBalance(seconduser);
  const transactionReward = await contracts.harvest.claimreward(seconduser, { authorization: `${seconduser}@active` })
  var balanceAfter = await getBalance(seconduser);

  assert({
    given: 'user received 10 refund',
    should: 'after balance should be 10 bigger than before',
    actual: balanceAfter,
    expected: balanceBefore + 10
  })

  const rewards = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'harvest',
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
    given: 'claim reward transaction',
    should: 'call inline action to history',
    actual: transactionReward.processed.action_traces[0].inline_traces[1].act.account,
    expected: history
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
    actual: rewards.rows.map(row => ({
      account: row.account,
      score: row.planted_score
    })),
    expected: [{
      account: firstuser,
      score: 100
    }, {
      account: seconduser,
      score: 50
    }]
  })

  assert({
    given: 'transactions calculation',
    should: 'assign transactions score to each user',
    actual: rewards.rows.map(({ transactions_score }) => transactions_score).sort((a, b) => b - a),
    expected: [100, 50]
  })

  assert({
    given: 'reputation calculation',
    should: 'assign reputation multiplier to each user',
    actual: rewards.rows.map(({ reputation_score }) => reputation_score),
    expected: [50, 100]
  })

  assert({
    given: 'harvest process',
    should: 'distribute rewards based on contribution scores'
  })

  assert({
    given: 'payforcpu called',
    should: 'work only when both authorizations are provided and user is seeds user',
    actual: [payforCPURequireAuth, payforCPURequireUserAuth, payforcpuSeedsUserOnly],
    expected: [true, true, true]
  })

})

describe("harvest contribution score", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, token, harvest, settings })


  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('plant seeds')
  await contracts.token.transfer(firstuser, harvest, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, harvest, '200.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  await contracts.harvest.calcplanted({ authorization: `${harvest}@active` })
  await contracts.harvest.calcrep({ authorization: `${harvest}@active` })
  await contracts.harvest.calctrx({ authorization: `${harvest}@active` })

  const balances = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'balances',
    json: true,
    limit: 100
  })

  const harvestStats = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'harvest',
    json: true,
    limit: 100
  })

  assert({
    given: 'planted calculation',
    should: 'have valies',
    actual: harvestStats.rows.map(({ planted_score }) => planted_score),
    expected: [100, 50]
  })

})

describe.only("plant for other user", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

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
  console.log('planted: ' + JSON.stringify(plantedBalances, null, 2))

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
