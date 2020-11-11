const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts, isLocal } = require('../scripts/helper');
const { expect } = require('chai');

const { harvest, accounts, proposals, settings, escrow, token, campaignbank, milestonebank, alliancesbank, firstuser, seconduser, thirduser, fourthuser, fifthuser, sixthuser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe('Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('prop.al.min', 500 * 10000, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  const cyclesTable = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'cycle',
    json: true
  })
  const initialCycle = cyclesTable.rows[0] ? cyclesTable.rows[0].propcycle : 0

  console.log('create proposal '+campaignbank)

  await contracts.proposals.createx(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })

  console.log('create another proposal')
  await contracts.proposals.createx(seconduser, seconduser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })

  const numberOfProposals = async () => {
    const props = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'props',
      json: true,
    })
    return props.rows.length
  }
  
  console.log('create and cancel proposal')
  await contracts.proposals.createx(firstuser, firstuser, '200.0000 SEEDS', 'prop to cancel', 'will be canceled', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  
  const numberOfProposalsBeforeCancel = await numberOfProposals()

  const balanceBeforeCancel = await getBalance(firstuser)
  await contracts.proposals.cancel(4, { authorization: `${firstuser}@active` })
  const balanceAfterCancel = await getBalance(firstuser)

  const numberOfProposalsAfterCancel = await numberOfProposals()

  console.log('create alliance proposal')
  await contracts.proposals.createx(fourthuser, fourthuser, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, [ 10, 30, 30, 30 ], { authorization: `${fourthuser}@active` })

  let notOwnerStake = true
  try {
    await contracts.token.transfer(seconduser, proposals, '50.0000 SEEDS', '4', { authorization: `${seconduser}@active` })
    notOwnerStake = false
  } catch(err) {
    console.log('stake from not owner (failed)')
  }

  console.log('update proposal')
  await contracts.proposals.update(1, 'title2', 'summary2', 'description2', 'image2', 'url2', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '2', { authorization: `${firstuser}@active` })

  console.log('deposit stake (without memo)')
  await contracts.token.transfer(seconduser, proposals, '500.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('deposit stake (without memo)')
  await contracts.token.transfer(fourthuser, proposals, '500.0000 SEEDS', '', { authorization: `${fourthuser}@active` })

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  let voteBeforePropIsActiveHasError = true
  try {
    await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })
    voteBeforePropIsActiveHasError = false
  } catch (err) {
    console.log('favour first proposal (failed)')
  }

  try {
    await contracts.proposals.against(firstuser, 2, 1, { authorization: `${firstuser}@active` })
    voteBeforePropIsActiveHasError = false
  } catch (err) {
    console.log('against second proposal (failed)')
  }

  let stakeMoreThanMax = false
  try {
    await contracts.token.transfer(firstuser, proposals, '75000.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
    stakeMoreThanMax = true
  } catch (err) {
    console.log('can not stake more than max value (expected)')
  }

  console.log('update contribution score of citizens')
  await contracts.harvest.testupdatecs(firstuser, 20, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 40, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 60, { authorization: `${harvest}@active` })

  console.log('1 move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  const activeProposals = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true,
  })

  const theAllianceProp = activeProposals.rows.filter( item => item.fund == "allies.seeds")[0]

  let activeProps = activeProposals.rows.filter( item => item.stage == "active")

  const voiceBefore = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })
  let voice = voiceBefore.rows[0].balance

  console.log('favour first proposal')
  await contracts.proposals.favour(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.against(firstuser, 1, 2, { authorization: `${firstuser}@active` })

  console.log('favour second proposal but not enough for 80% majority')
  await contracts.proposals.against(seconduser, 2, 4, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, 2, 6, { authorization: `${firstuser}@active` })

  console.log('favour alliance proposal ' + theAllianceProp.id)
  await contracts.proposals.favour(seconduser, theAllianceProp.id, 4, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, theAllianceProp.id, 4, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(thirduser, theAllianceProp.id, 4, { authorization: `${thirduser}@active` })

  var exceedBalanceHasError = true
  try {
    await contracts.proposals.against(thirduser, 1, 100, { authorization: `${thirduser}@active` })
    exceedBalanceHasError = false
  } catch (err) {
    console.log('tried to spend more voice than they have - fails')
  }

  var voteASecondTimeHasError = true
  try {
    await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })
    voteASecondTimeHasError = false
  } catch (err) {
    console.log('vote a second time - fails')
  }

  console.log('against second proposal')
  await contracts.proposals.against(firstuser, 3, 1, { authorization: `${firstuser}@active` })

  const balancesBefore = [
    await getBalance(firstuser),
    await getBalance(seconduser),
    await getBalance(campaignbank),
  ]

  console.log('new citizen')
  await contracts.accounts.testcitizen(fourthuser, { authorization: `${accounts}@active` })
  await contracts.proposals.addvoice(fourthuser, 0, { authorization: `${proposals}@active` })
  await contracts.harvest.testupdatecs(fourthuser, 80, { authorization: `${harvest}@active` })

  const voice111 = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })

  const repsBefore = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    lower: firstuser,
    upper: firstuser,
    json: true,
  })

  console.log('execute proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  const repsAfter = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    lower: firstuser,
    upper: firstuser,
    json: true,
  })
  console.log("reps: "+JSON.stringify(repsAfter))

  await sleep(2000)
  
  const voiceAfter = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })
  
  const hasVoice = (voices, user) => {
    return voices.rows.filter(
      (item) => item.account == user
    ).length == 1
  }

  const balancesAfter = [
    await getBalance(firstuser),
    await getBalance(seconduser),
    await getBalance(campaignbank),
  ]


  const escrowLocks = await eos.getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true,
  })

  //console.log("escrow: "+JSON.stringify(escrowLocks, null, 2))
  
  const { rows } = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  delete rows[0].creation_date
  delete rows[1].creation_date
  delete rows[2].creation_date

  console.log('Keep evaluating proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  const propTableAfterFinish = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  const balancesAfterFinish = [
    await getBalance(firstuser),
    await getBalance(fourthuser),
    await getBalance(campaignbank),
  ]

  console.log(balancesBefore, balancesAfterFinish)

  assert({
    given: 'max stake exceeded',
    should: 'fail',
    actual: stakeMoreThanMax,
    expected: false
  })

  assert({
    given: 'try to vote before proposal is active',
    should: 'fail',
    actual: voteBeforePropIsActiveHasError,
    expected: true
  })

  let balances = voiceAfter.rows.map( ({ balance })=>balance )
  assert({
    given: 'voice reset after onperiod',
    should: 'have amount of voice proportional to contribution score (first account)',
    actual: balances,
    expected: [20, 40, 60, 80]
  })

  assert({
    given: 'exceeded voice balance',
    should: 'throw error',
    actual: exceedBalanceHasError,
    expected: true
  })

  assert({
    given: 'vote a second time',
    should: 'has error',
    actual: voteASecondTimeHasError,
    expected: true
  })

  assert({
    given: 'passed proposal',
    should: 'send reward and stake',
    actual: balancesAfter[0] - balancesBefore[0],
    expected: 510
  })

  assert({
    given: 'passed proposal',
    should: 'gain rep',
    actual: repsAfter.rows[0].rep - repsBefore.rows[0].rep,
    expected: 10
  })

  assert({
    given: 'rejected proposal',
    should: 'burn staked tokens',
    actual: balancesAfter[1] - balancesBefore[1],
    expected: 0
  })

  assert({
    given: 'stake from not owner',
    should: 'has error',
    actual: notOwnerStake,
    expected: true
  })

  assert({
    given: 'cancel proposal',
    should: 'refund staked tokens',
    actual: balanceAfterCancel - balanceBeforeCancel,
    expected: 50
  })

  assert({
    given: 'before cancel proposal',
    should: 'should have 4 proposals',
    actual: numberOfProposalsBeforeCancel,
    expected: 4
  })
  
  assert({
    given: 'after cancel proposal',
    should: 'should have 3 proposals',
    actual: numberOfProposalsAfterCancel,
    expected: 3
  })

  assert({
    given: 'passed proposal 80% majority',
    should: 'go to evaluate state',
    actual: rows[0],
    expected: {
      id: 1,
      creator: firstuser,
      recipient: firstuser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 10,
      favour: 8,
      against: 2,
      stage: 'active',
      title: 'title2',
      summary: 'summary2',
      description: 'description2',
      image: 'image2',
      url: 'url2',
      status: 'evaluate',
      fund: campaignbank,
      pay_percentages: [10,30,30,30],
      passed_cycle: initialCycle + 1,
      age: 0,
      current_payout: '10.0000 SEEDS'
    }
  })

  assert({
    given: 'rejected proposal 60% majority',
    should: 'show rejected proposal',
    actual: rows[1],
    expected: {
      id: 2,
      creator: firstuser,
      recipient: firstuser,
      quantity: '55.7000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 10,
      favour: 6,
      against: 4,
      stage: 'done',
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      fund: campaignbank,
      pay_percentages: [10,30,30,30],
      passed_cycle: 0,
      age: 0,
      current_payout: '0.0000 SEEDS'
    }
  })

  assert({
    given: 'rejected proposal',
    should: 'show rejected proposal',
    actual: rows[2],
    expected: {
      id: 3,
      creator: seconduser,
      recipient: seconduser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 1,
      favour: 0,
      against: 1,
      stage: 'done',
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      fund: campaignbank,
      pay_percentages: [10,30,30,30],
      passed_cycle: 0,
      age: 0,
      current_payout: '0.0000 SEEDS'
    }
  })

  assert({
    given: 'executed proposal',
    should: 'decrease bank balance',
    actual: balancesAfter[2] - balancesBefore[2],
    expected: -10
  })

  assert({
    given: 'new citizen was added',
    should: 'be added to voice on new cycle',
    actual: [hasVoice(voiceBefore, fourthuser), hasVoice(voiceAfter, fourthuser)],
    expected: [false, true]
  })

  let escrowLock = escrowLocks.rows[0]

  delete escrowLock.vesting_date
  delete escrowLock.created_date
  delete escrowLock.updated_date

  assert({
    given: 'alliance proposal passed',
    should: 'have entry in escrow locks',
    actual: escrowLock,
    expected: {
      "id": 0,
      "lock_type": "event",
      "sponsor": "allies.seeds",
      "beneficiary": "seedsuserxxx",
      "quantity": "1.2000 SEEDS",
      "trigger_event": "golive",
      "trigger_source": "dao.hypha",
      "notes": "proposal id: 4",
    }
  })

  delete propTableAfterFinish.rows[0].creation_date
  delete propTableAfterFinish.rows[3].creation_date

  assert({
    given: 'finished cycle number 4',
    should: 'complete execution of proposal 1',
    expected: {
      id: 1,
      creator: firstuser,
      recipient: firstuser,
      quantity: '100.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 1,
      total: 10,
      favour: 8,
      against: 2,
      title: 'title2',
      summary: 'summary2',
      description: 'description2',
      image: 'image2',
      url: 'url2',
      status: 'passed',
      stage: 'done',
      fund: 'gift.seeds',
      pay_percentages: [10,30,30,30],
      passed_cycle: initialCycle + 1,
      age: 3,
      current_payout: '100.0000 SEEDS'
    },
    actual: propTableAfterFinish.rows[0]
  })

  assert({
    given: 'finished cycle number 3',
    should: 'complete execution of proposal 4',
    expected: {
      id: 4,
      creator: fourthuser,
      recipient: fourthuser,
      quantity: '12.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 1,
      total: 12,
      favour: 12,
      against: 0,
      title: 'alliance',
      summary: 'test alliance',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'passed',
      stage: 'done',
      fund: 'allies.seeds',
      pay_percentages: [10,30,30,30],
      passed_cycle: initialCycle + 1,
      age: 3,
      current_payout: '12.0000 SEEDS'
    },
    actual: propTableAfterFinish.rows[3]
  })

  assert({
    given: 'finish proposal\'s cycles',
    should: 'send reward',
    actual: balancesAfterFinish[0] - balancesBefore[0],
    expected: 600
  })

  const escrowLocksAfterFinish = await eos.getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true,
  })
  console.log('escrow locks:', escrowLocksAfterFinish)

})

describe('Evaluation phase', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  console.log('create proposal '+campaignbank)
  await contracts.proposals.createx(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, firstuser, '200.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 25, 25, 25, 25 ], { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '2', { authorization: `${firstuser}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })

  console.log('favour first proposal')
  await contracts.proposals.favour(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(thirduser, 1, 8, { authorization: `${thirduser}@active` })

  console.log('vote on proposal 2')
  await contracts.proposals.favour(firstuser, 2, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.against(seconduser, 2, 1, { authorization: `${seconduser}@active` })

  console.log('move proposals to evaluation phase')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })

  const testQuantity = async (expectedValues) => {
    const proposalsTable = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'props',
      json: true,
    })
    assert({
      given: 'onperiod ran',
      should: 'give correct amount to proposal',
      actual: proposalsTable.rows.map(r => r.current_payout),
      expected: expectedValues
    })    
  }

  await testQuantity(['10.0000 SEEDS', '50.0000 SEEDS'])

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)
  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })

  await testQuantity(['40.0000 SEEDS', '100.0000 SEEDS'])

  let changeVote1 = false
  try {
    await contracts.proposals.against(thirduser, 2, 8, { authorization: `${thirduser}@active` })
    changeVote1 = true
  } catch (err) {
    console.log('user that did not vote, tries to vote in evaluate phase (fails)')
  }

  let changeVote2 = false
  try {
    await contracts.proposals.favour(seconduser, 2, 8, { authorization: `${seconduser}@active` })
    changeVote1 = true
  } catch (err) {
    console.log('user that voted against, tries to vote in evaluate phase (fails)')
  }

  let changeVote3 = false
  try {
    await contracts.proposals.against(seconduser, 2, 8, { authorization: `${seconduser}@active` })
    changeVote1 = true
  } catch (err) {
    console.log('user that voted against, tries to vote in evaluate phase (fails)')
  }

  await contracts.proposals.against(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.against(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.against(thirduser, 1, 8, { authorization: `${thirduser}@active` })

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await testQuantity(['40.0000 SEEDS', '150.0000 SEEDS'])

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await testQuantity(['40.0000 SEEDS', '200.0000 SEEDS'])

  assert({
    given: 'trying to vote in evaluate state',
    should: 'fail',
    actual: changeVote1,
    expected: false
  })

  assert({
    given: 'trying to vote in evaluate state',
    should: 'fail',
    actual: changeVote2,
    expected: false
  })

  assert({
    given: 'trying to vote in evaluate state',
    should: 'fail',
    actual: changeVote3,
    expected: false
  })

})


describe('Participants', async assert => {
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  await sleep(2000)

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })

  console.log('create proposal '+campaignbank)
  await contracts.proposals.create(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [10, 30, 30, 30], { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '2', { authorization: `${firstuser}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(10000)

  const participantsBefore = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'participants',
    json: true,
  })

  const reputationBefore = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  console.log('favour first proposal')
  await contracts.proposals.favour(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.against(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.neutral(firstuser, 2, { authorization: `${firstuser}@active` })
  await contracts.proposals.neutral(thirduser, 1, { authorization: `${thirduser}@active` })
  await contracts.proposals.neutral(thirduser, 2, { authorization: `${thirduser}@active` })

  const votes = await eos.getTableRows({
    code: proposals,
    scope: 1,
    table: 'votes',
    json: true,
  })

  const voiceAfter = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })

  const participantsAfter = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'participants',
    json: true,
  })

  console.log('erase participants')
  await sleep(10000)
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(10000)

  const reputationAfter = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const participantsAfterOnPeriod = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'participants',
    json: true,
  })

  assert({
    given: 'voice after voting',
    should: 'have the correct voice amount',
    actual: voiceAfter.rows,
    expected: [
      { account: firstuser, balance: 12 },
      { account: seconduser, balance: 32 },
      { account: thirduser, balance: 60 }
    ]
  })

  assert({
    given: 'voted for a proposal',
    should: 'have votes entry',
    actual: votes.rows,
    expected: [
      { proposal_id: 1, account: firstuser, amount: 8, favour: 0 },
      { proposal_id: 1, account: seconduser, amount: 8, favour: 1 },
      { proposal_id: 1, account: thirduser, amount: 0, favour: 0 }
    ]
  })

  assert({
    given: 'before voting',
    should: 'have no participants entries',
    actual: participantsBefore.rows,
    expected: []
  })

  assert({
    given: 'after voting',
    should: 'have participants entries',
    actual: participantsAfter.rows,
    expected: [
      { account: firstuser, nonneutral: 1, count: 2 },
      { account: seconduser, nonneutral: 1, count: 1 },
      { account: thirduser, nonneutral: 0, count: 2 }
    ]
  })

  assert({
    given: 'after on period',
    should: 'have no participants entries',
    actual: participantsAfterOnPeriod.rows,
    expected: []
  })

  assert({
    given: 'before voting',
    should: 'not have reputation',
    actual: reputationBefore.rows.map(({reputation}) => reputation),
    expected: [ 0, 0, 0 ]
  })

  assert({
    given: 'after voting',
    should: 'have reputation',
    actual: reputationAfter.rows.map(({reputation}) => reputation),
    expected: [ 6, 1, 1 ]
  })
})


describe('Change Trust', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })

  let check = async (given, should, expected) => {
    const voice = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'voice',
      json: true,
    })
    //console.log('given ' + given + " : "  + JSON.stringify(voice))
    assert({
      given: given,
      should: should,
      actual: voice.rows.length,
      expected: expected
    })

  }

  await check("before", "empty", 0) 

  await contracts.proposals.changetrust(firstuser, 1, { authorization: `${proposals}@active` })

  await check("after", "has voice", 1) 

  await contracts.proposals.changetrust(firstuser, 0, { authorization: `${proposals}@active` })

  await check("off", "no voice", 0) 

  //await contracts.proposals.changetrust(firstuser, 1, { authorization: `${proposals}@active` })

  //await check("after", "has voice", 1) 

  

})

describe('Proposals Quorum', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('set settings')
  await contracts.settings.configure("prop.cmp.min", 2 * 10000, { authorization: `${settings}@active` })

  // tested with 25 - pass, pass
  // 33 - fail, pass
  // 50 - fail, pass
  // 51 - fail, fail
  await contracts.settings.configure("propquorum", 33, { authorization: `${settings}@active` }) 
  await contracts.settings.configure("propmajority", 50, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fifthuser, 'fifthuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(sixthuser, 'sixthuser', 'individual', { authorization: `${accounts}@active` })

  console.log('create proposal')
  await contracts.proposals.createx(firstuser, firstuser, '2.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, firstuser, '2.5000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(seconduser, seconduser, '1.4000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '2', { authorization: `${firstuser}@active` })
  console.log('deposit stake 3')
  await contracts.token.transfer(seconduser, proposals, '2.0000 SEEDS', '3', { authorization: `${seconduser}@active` })
  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  let users = [firstuser, seconduser, thirduser, fourthuser, fifthuser, sixthuser]
  for (i = 0; i<users.length; i++ ) {
    let user = users[i]
    console.log('add voice '+user)
    await contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })
    await contracts.proposals.addvoice(user, 44, { authorization: `${proposals}@active` })
  }

  console.log('vote on first proposal')
  await contracts.proposals.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })

  console.log('vote on second proposal')
  await contracts.proposals.favour(seconduser, 2, 3, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, 2, 3, { authorization: `${firstuser}@active` })

  console.log('execute proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  const props = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  const testQuorum = async (numberProposals, expectedValue) => {
    try {
      await contracts.proposals.testquorum(numberProposals, { authorization: `${proposals}@active` })
    } catch (err) {
      const e = JSON.parse(err)
      assert({
        given: 'get quorum called',
        should: 'give the correct quorum threshold',
        expected: `assertion failure with message: ${expectedValue}`,
        actual: e.error.details[0].message
      })
    }
  }

  //console.log("props "+JSON.stringify(props, null, 2))

  const min = 7
  const max = 20
  await testQuorum(0, 5)
  await testQuorum(1, 20)
  await testQuorum(2, 20)
  await testQuorum(5, 18)
  await testQuorum(10, 9)
  await testQuorum(20, 5)

  assert({
    given: 'failed proposal quorum',
    should: 'be rejected',
    actual: props.rows[0].status,
    expected: 'rejected'
  })


  assert({
    given: 'passed proposal quorum majority',
    should: 'have passed',
    actual: props.rows[1].status,
    expected: "evaluate"
  })

})

describe('Recepient invalid', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })

  console.log('create proposal')
  var createdProp = false
  try {
    await contracts.proposals.createx(firstuser, "23", '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
    createdProp = true
    console.log('error')
  } catch (err) {

  }

  console.log('create proposal with invalid fund')
  var createdFundProp = false
  try {
    await contracts.proposals.createx(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', "clowns", [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
    console.log('error')
    createdFundProp = true
  } catch (err) {

  }

  console.log('create proposal with non seeds user')
  var createNonSeedsUser = false
  try {
    await contracts.proposals.createx(milestonebank, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, 10, 3, 'linear', { authorization: `${firstuser}@active` })
    console.log('error')
    createNonSeedsUser = true
  } catch (err) {

  }


  assert({
    given: 'create proposal with invalid recepient',
    should: 'fail',
    actual: createdProp,
    expected: false
  })
  
  assert({
    given: 'create proposal with invalid fund',
    should: 'fail',
    actual: createdFundProp,
    expected: false
  })

  assert({
    given: 'create proposal with non seeds user',
    should: 'fail',
    actual: createNonSeedsUser,
    expected: false
  })

})


describe('Stake limits', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('token reset')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  console.log('create proposal '+campaignbank)
  await contracts.proposals.createx(firstuser, firstuser, '1000.0000 SEEDS', '1000 seeds please', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(seconduser, seconduser, '100000.0000 SEEDS', '100,0000 seeds please', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })
  await contracts.proposals.createx(thirduser, thirduser, '10000000.0000 SEEDS', '1,000,000 seeds please', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${thirduser}@active` })

  console.log('stake the minimum')
  await contracts.token.transfer(firstuser, proposals, '554.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  
  let expectNotEnough = true
  try {
    await contracts.proposals.checkstake(1, { authorization: `${firstuser}@active` })
    expectNotEnough = false
  } catch (err) {

  }
  await contracts.token.transfer(firstuser, proposals, '1.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.proposals.checkstake(1, { authorization: `${firstuser}@active` })

  console.log('stake 5% = 5000')
  await contracts.token.transfer(seconduser, proposals, '555.0000 SEEDS', '', { authorization: `${seconduser}@active` })
  let expectNotEnough2 = true
  try {
    await contracts.proposals.checkstake(2, { authorization: `${firstuser}@active` })
    expectNotEnough2 = false
  } catch (error) {
    //console.log("expected: "+error)
  }
  await contracts.token.transfer(seconduser, proposals, (5000-555) + '.0000 SEEDS', '', { authorization: `${seconduser}@active` })
  await contracts.proposals.checkstake(2, { authorization: `${firstuser}@active` })

  console.log('stake max - not more than 75,000 needed')
  await contracts.token.transfer(thirduser, proposals, '75000.0000 SEEDS', '', { authorization: `${thirduser}@active` })
  await contracts.proposals.checkstake(3, { authorization: `${firstuser}@active` })

  await contracts.proposals.createx(fourthuser, fourthuser, '2.0000 SEEDS', '2 seeds please', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${fourthuser}@active` })
  await contracts.token.transfer(fourthuser, proposals, '2.0000 SEEDS', '', { authorization: `${fourthuser}@active` })
  let expectNotEnough3 = true
  try {
    await contracts.proposals.checkstake(4, { authorization: `${firstuser}@active` })
    expectNotEnough3 = false
  } catch (error) {
    //console.log("expected: "+error)
  }

  await contracts.proposals.onperiod( { authorization: `${proposals}@active` } )

  const activeProposals = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true,
  })

  console.log('create proposal '+alliancesbank)
  await contracts.proposals.create(seconduser, seconduser, '100000.0000 SEEDS', '100,0000 seeds please', 'summary', 'description', 'image', 'url', alliancesbank, { authorization: `${seconduser}@active` })
  await contracts.proposals.create(thirduser, thirduser, '100000000.0000 SEEDS', '100,000,000 seeds please', 'summary', 'description', 'image', 'url', alliancesbank, { authorization: `${thirduser}@active` })

  await contracts.token.transfer(seconduser, proposals, '999.0000 SEEDS', '5', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, proposals, '14999.0000 SEEDS', '6', { authorization: `${thirduser}@active` })

  let expectNotEnough4 = true
  try {
    await contracts.proposals.checkstake(5, { authorization: `${seconduser}@active` })
    expectNotEnough4 = false
  } catch (error) {
    //console.log("expected: "+error)
  }

  let expectNotEnough5 = true
  try {
    await contracts.proposals.checkstake(6, { authorization: `${thirduser}@active` })
    expectNotEnough5 = false
  } catch (error) {
    //console.log("expected: "+error)
  }

  await contracts.token.transfer(seconduser, proposals, '1.0000 SEEDS', '5', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, proposals, '1.0000 SEEDS', '6', { authorization: `${thirduser}@active` })

  await contracts.proposals.checkstake(5, { authorization: `${seconduser}@active` })
  await contracts.proposals.checkstake(6, { authorization: `${seconduser}@active` })

  const minStakes = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'minstake',
    json: true,
  })

  console.log("min stake "+JSON.stringify(minStakes, null, 2))

  assert({
    given: 'proposal not having enough stake',
    should: 'fail',
    actual: expectNotEnough,
    expected: true
  })
  
  assert({
    given: 'proposal 2 not having enough stake',
    should: 'fail',
    actual: expectNotEnough2,
    expected: true
  })

  assert({
    given: 'proposal 4 not having enough stake',
    should: 'fail',
    actual: expectNotEnough3,
    expected: true
  })

  assert({
    given: 'proposal 5 not having enough stake',
    should: 'fail',
    actual: expectNotEnough4,
    expected: true
  })

  assert({
    given: 'proposal 6 not having enough stake',
    should: 'fail',
    actual: expectNotEnough5,
    expected: true
  })

  assert({
    given: '3 proposals with enough stake, one without',
    should: '3 active, one staged',
    actual: activeProposals.rows.map( ({stage}) => stage ),
    expected: ['active', 'active', 'active', 'staged']
  })

  assert({
    given: 'min stakes filled in',
    should: 'have the right values',
    actual: minStakes.rows,
    expected: [
      {
        "prop_id": 1,
        "min_stake": 5550000
      },
      {
        "prop_id": 2,
        "min_stake": 25000000
      },
      {
        "prop_id": 3,
        "min_stake": 750000000
      },
      {
        "prop_id": 4,
        "min_stake": 5550000
      },
      {
        "prop_id": 5,
        "min_stake": 10000000
      },
      {
        "prop_id": 6,
        "min_stake": 150000000
      }
    ]
  })


})

describe('Demote inactive citizens', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('token reset')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  const testActives = async expectedValues => {
    const actives = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'actives',
      json: true,
    })
    assert({
      given: 'test active',
      should: '',
      expected: expectedValues,
      actual: actives.rows.map(r => {
        return {
          account: r.account,
          active: r.active
        }
      })
    })
  }

  const testActiveSize = async expectedValues => {
    const sizes = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'sizes',
      json: true,
    })
    const activeArray = sizes.rows.filter(r => r.id === 'active.sz')
    const activeSize = activeArray.length > 0 ? activeArray[0].size : 0
    assert({
      given: 'test active size',
      should: '',
      expected: expectedValues,
      actual: activeSize
    })
  }
  
  const testUserStatus = async expectedValues => {
    const users = await eos.getTableRows({
      code: accounts,
      scope: accounts,
      table: 'users',
      json: true,
    })
    const userStatus = users.rows.map(u => u.status)
    assert({
      given: 'test user status',
      should: 'have the expected status',
      expected: expectedValues,
      actual: userStatus
    })
  }

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  const now = parseInt(Date.now() / 1000)

  const actives = [
    { account: 'seedsuseraaa', active: 1 },
    { account: 'seedsuserbbb', active: 1 },
    { account: 'seedsuserccc', active: 1 }
  ]
  const users = [
    'citizen',
    'citizen',
    'citizen',
    'visitor'
  ]

  console.log('testActives')
  await testActives(actives)
  await testActiveSize(3)

  await sleep(2000)
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })

  console.log('testActives 2')
  await testActives(actives)
  await testActiveSize(3)
  await testUserStatus(users)

  console.log('createx 2')
  await contracts.proposals.createx(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  await sleep(8000)

  console.log('favour')
  await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })

  const activesAfterVote = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'actives',
    json: true,
  })
  const firstActive = activesAfterVote.rows.filter(a => a.account === firstuser)
  const secondActive = activesAfterVote.rows.filter(a => a.account === seconduser)
  
  console.log('mooncyclesec')
  await contracts.settings.configure('mooncyclesec', 3, { authorization: `${settings}@active` })
  await sleep(2000)
  console.log('onper 1')

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)
  console.log('onper 2')

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  actives[0].active = 0
  actives[2].active = 0
  await testActives(actives)
  await testActiveSize(1)
  users[0] = 'resident'
  users[2] = 'resident'
  console.log('testUserStatus 2')

  await testUserStatus(users)

  console.log('user comes back from resident')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  actives[0].active = 1
  actives[2].active = 0
  await testActives(actives)
  await testActiveSize(2)
  users[0] = 'citizen'
  users[2] = 'resident'
  await testUserStatus(users)

  await sleep(2000)
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)
  actives[0].active = 0
  actives[2].active = 0
  await testActives(actives)
  await testActiveSize(1)
  users[0] = 'resident'
  users[2] = 'resident'
  await testUserStatus(users)

  console.log("set decay to 2 seconds")
  await contracts.settings.configure('decaytime', 2, { authorization: `${settings}@active` })
  
  console.log("give contribution score and voice to users")
  await contracts.harvest.testupdatecs(firstuser, 50, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 80, { authorization: `${harvest}@active` })

  console.log("user 1 comes back to citizen from being demoted")
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })

  console.log("manually set voice decay time to now")
  await contracts.proposals.testvdecay(parseInt(Date.now() / 1000), { authorization: `${proposals}@active` })
  await contracts.settings.configure('propdecaysec', 1, { authorization: `${settings}@active` })

  console.log("user 3 comes back to citizen from being demoted")
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })
  const voiceFirstUser = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })

  assert({
    given: 'voted',
    should: 'update active timestamp',
    expected: true,
    actual: (secondActive[0].timestamp > now + 6) && (Math.abs(firstActive[0].timestamp - now) <= 1)
  })

  assert({
    given: 'voice recovered',
    should: 'have the correct amount of voice',
    expected: [
      { account: 'seedsuseraaa', balance: 50 },
      { account: 'seedsuserccc', balance: 57 }
    ],
    actual: voiceFirstUser.rows.filter(r => r.account == firstuser || r.account == thirduser)
  })

})


describe('Voice decay', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })

  console.log('change decaytime')
  await contracts.settings.configure('decaytime', 30, { authorization: `${settings}@active` })

  console.log('propdecaysec')
  await contracts.settings.configure('propdecaysec', 5, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('rank css')
  await contracts.harvest.testupdatecs(firstuser, 40, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 89, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 0, { authorization: `${harvest}@active` })
  await sleep(2000)

  const testVoiceDecay = async (expectedValues, n) => {
    console.log('voice decay')
    await contracts.proposals.decayvoices({ authorization: `${proposals}@active` })
    await sleep(2000)

    const voice = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'voice',
      json: true,
    })

    assert({
      given: 'ran voice decay for the ' + n + ' time',
      should: 'decay voices if required',
      actual: voice.rows.map(r => r.balance),
      expected: expectedValues
    })
  }

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(4000)

  await testVoiceDecay([40, 89, 0], 1)
  await sleep(10000)
  await testVoiceDecay([40, 89, 0], 2)
  await sleep(15000)
  await testVoiceDecay([34, 75, 0], 3)
  await sleep(1000)
  await testVoiceDecay([34, 75, 0], 4)
  await sleep(4000)
  await testVoiceDecay([28, 63, 0], 5)
  await sleep(2000)

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(4000)

  await testVoiceDecay([40, 89, 0], 6)

})
