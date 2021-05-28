const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts, isLocal, ramdom64ByteHexString, fromHexString, sha256 } = require('../scripts/helper');
const { expect } = require('chai');

const { 
  harvest, accounts, proposals, settings, escrow, token, organization, onboarding, pool,
  campaignbank, milestonebank, alliancesbank, hyphabank,
  firstuser, seconduser, thirduser, fourthuser, fifthuser
} = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

let eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

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
  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })

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

  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(fourthuser, { authorization: `${accounts}@active` })

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
  await contracts.proposals.createx(fourthuser, fourthuser, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, [], { authorization: `${fourthuser}@active` })

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
  await contracts.harvest.testupdatecs(firstuser, 90, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 90, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 90, { authorization: `${harvest}@active` })

  console.log('1 move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  console.log('------------------------------------')
  const cyclestats1 = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'cyclestats',
    json: true,
  })
  console.log("cycle stats 1",cyclestats1)

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

  console.log("voice "+JSON.stringify(voice, null, 2))

  console.log('favour first proposal')
  await contracts.proposals.favour(seconduser, 1, 40, { authorization: `${seconduser}@active` })
  await contracts.proposals.against(firstuser, 1, 10, { authorization: `${firstuser}@active` })

  console.log('favour second proposal but not enough for 80% majority')
  await contracts.proposals.against(seconduser, 2, 20, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, 2, 30, { authorization: `${firstuser}@active` })

  console.log('favour alliance proposal ' + theAllianceProp.id)
  await contracts.proposals.favour(seconduser, theAllianceProp.id, 20, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, theAllianceProp.id, 20, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(thirduser, theAllianceProp.id, 20, { authorization: `${thirduser}@active` })

  var exceedBalanceHasError = true
  try {
    await contracts.proposals.against(thirduser, 90, 100, { authorization: `${thirduser}@active` })
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
  await sleep(3000)

  //console.log('------------------------------------')
  const cyclestats2 = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'cyclestats',
    json: true,
  })
  //console.log("cycle stats 2",cyclestats2)

  const repsAfter = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    lower: firstuser,
    upper: firstuser,
    json: true,
  })
  //console.log("reps: "+JSON.stringify(repsAfter))

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

  console.log("PROPS: "+JSON.stringify(rows, null, 2))

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
    expected: [90, 90, 90, 80]
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
    expected: 15
  })

  assert({
    given: 'rejected proposal',
    should: 'burn staked tokens',
    actual: balancesAfter[1] - balancesBefore[1],
    expected: 0
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
      total: 50,
      favour: 40,
      against: 10,
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
      current_payout: '10.0000 SEEDS',
      campaign_type: 'cmp.funding',
      max_amount_per_invite: '0.0000 SEEDS',
      planted: '0.0000 SEEDS',
      reward: '0.0000 SEEDS',
      campaign_id: 0
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
      total: 50,
      favour: 30,
      against: 20,
      stage: 'done',
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      fund: campaignbank,
      pay_percentages: [10,30,30,30],
      passed_cycle: 1,
      age: 0,
      current_payout: '0.0000 SEEDS',
      campaign_type: 'cmp.funding',
      max_amount_per_invite: '0.0000 SEEDS',
      planted: '0.0000 SEEDS',
      reward: '0.0000 SEEDS',
      campaign_id: 0
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
      passed_cycle: 1,
      age: 0,
      current_payout: '0.0000 SEEDS',
      campaign_type: 'cmp.funding',
      max_amount_per_invite: '0.0000 SEEDS',
      planted: '0.0000 SEEDS',
      reward: '0.0000 SEEDS',
      campaign_id: 0
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

  console.log(escrowLock)

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
      "quantity": "12.0000 SEEDS",
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
      total: 50,
      favour: 40,
      against: 10,
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
      current_payout: '100.0000 SEEDS',
      campaign_type: 'cmp.funding',
      max_amount_per_invite: '0.0000 SEEDS',
      planted: '0.0000 SEEDS',
      reward: '0.0000 SEEDS',
      campaign_id: 0
    },
    actual: propTableAfterFinish.rows[0]
  })

  assert({
    given: 'finished cycle number 3, proposal 4',
    should: 'still be active',
    expected: {
      id: 4,
      creator: fourthuser,
      recipient: fourthuser,
      quantity: '12.0000 SEEDS',
      staked: '0.0000 SEEDS',
      executed: 0,
      total: 60,
      favour: 60,
      against: 0,
      title: 'alliance',
      summary: 'test alliance',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'evaluate',
      stage: 'active',
      fund: 'allies.seeds',
      pay_percentages: [100],
      passed_cycle: initialCycle + 1,
      age: 4,
      current_payout: '12.0000 SEEDS',
      campaign_type: 'alliance',
      max_amount_per_invite: '0.0000 SEEDS',
      planted: '0.0000 SEEDS',
      reward: '0.0000 SEEDS',
      campaign_id: 0
    },
    actual: propTableAfterFinish.rows[3]
  })

  assert({
    given: 'finish proposal\'s cycles',
    should: 'send reward',
    actual: balancesAfterFinish[0] - balancesBefore[0],
    expected: 600
  })

  delete cyclestats1.rows[0].start_time
  delete cyclestats1.rows[0].end_time

  assert({
    given: 'onperiod executed',
    should: 'store cycle stats',
    actual: cyclestats1.rows,
    expected: [
      {
        propcycle: initialCycle + 1,
        num_proposals: 0,
        num_votes: 0,
        total_voice_cast: 0,
        total_favour: 0,
        total_against: 0,
        total_citizens: 3,
        quorum_vote_base: 0,
        quorum_votes_needed: 0,
        unity_needed: '0.80000001192092896',
        total_eligible_voters: 0,
        active_props: [ 1, 2, 3, 4 ],
        eval_props: []
      }
    ]
  })

  delete cyclestats2.rows[0].start_time
  delete cyclestats2.rows[0].end_time
  delete cyclestats2.rows[1].start_time
  delete cyclestats2.rows[1].end_time

  assert({
    given: 'onperiod executed',
    should: 'store cycle stats',
    actual: cyclestats2.rows,
    expected: [
      {
        propcycle: initialCycle + 1,
        num_proposals: 0,
        num_votes: 8,
        total_voice_cast: 161,
        total_favour: 130,
        total_against: 31,
        total_citizens: 3,
        quorum_vote_base: 0,
        quorum_votes_needed: 0,
        unity_needed: '0.80000001192092896',
        total_eligible_voters: 4,
        active_props: [ 1, 2, 3, 4 ],
        eval_props: []
      },
      {
        propcycle: initialCycle + 2,
        num_proposals: 0,
        num_votes: 0,
        total_voice_cast: 0,
        total_favour: 0,
        total_against: 0,
        total_citizens: 4,
        quorum_vote_base: 0,
        quorum_votes_needed: 0,
        unity_needed: '0.80000001192092896',
        total_eligible_voters: 0,
        active_props: [],
        eval_props: [ 1, 4 ]
      }
    ]
  })

  const votedProps = await getTableRows({
    code: proposals,
    scope: initialCycle + 1,
    table: 'cycvotedprps',
    json: true
  })

  assert({
    given: 'voted props in first cycle',
    should: 'have the correct entries',
    actual: votedProps.rows.map(r => r.proposal_id),
    expected: [1,2,3,4]
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

  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })
  console.log('disable quorum - support requirement')
  await contracts.settings.configure('quorum.base', 0, { authorization: `${settings}@active` })
  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })

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

  console.log(`add rep to ${firstuser}`)
  await contracts.accounts.addrep(firstuser, 100, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })

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

  const repsBefore = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    json: true,
  })

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await testQuantity(['40.0000 SEEDS', '150.0000 SEEDS'])

  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await testQuantity(['40.0000 SEEDS', '200.0000 SEEDS'])

  const repsAfter = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    json: true,
  })

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

  assert({
    given: 'proposal in eval state failed',
    should: 'lose rep',
    actual: repsAfter.rows[0].rep - repsBefore.rows[0].rep,
    expected: -100
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

  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })

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

  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })

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

  console.log('favour first proposal x')
  await contracts.proposals.addvoice(firstuser, 20, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 40, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 60, { authorization: `${proposals}@active` })

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
    const voiceAlliance = await eos.getTableRows({
      code: proposals,
      scope: 'alliance',
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
    assert({
      given: given,
      should: should,
      actual: voiceAlliance.rows.length,
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

describe('Proposals Quorum And Support Levels', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('set settings')
  await contracts.settings.configure("prop.cmp.min", 2 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("prop.al.min", 2 * 10000, { authorization: `${settings}@active` })
  
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

  console.log('create proposal')
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(seconduser, { authorization: `${accounts}@active` })

  await contracts.proposals.createx(firstuser, firstuser, '2.0000 SEEDS', '0', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, firstuser, '2.5000 SEEDS', '1', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(seconduser, seconduser, '1.4000 SEEDS', '2', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })
  await contracts.proposals.createx(seconduser, seconduser, '1.7777 SEEDS', '3', 'summary', 'description', 'image', 'url', alliancesbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '2', { authorization: `${firstuser}@active` })
  console.log('deposit stake 3')
  await contracts.token.transfer(seconduser, proposals, '2.0000 SEEDS', '3', { authorization: `${seconduser}@active` })
  console.log('deposit stake 4')
  await contracts.token.transfer(seconduser, proposals, '2.0000 SEEDS', '4', { authorization: `${seconduser}@active` })

  let users = [firstuser, seconduser, thirduser, fourthuser, fifthuser]
  for (i = 0; i<users.length; i++ ) {
    let user = users[i]
    console.log('make citizen '+user)
    await contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })
  }

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await sleep(1000)

  for (i = 0; i<users.length; i++ ) {
    let user = users[i]
    console.log('add voice '+user)
    await contracts.proposals.addvoice(user, 44, { authorization: `${proposals}@active` })
  }

  console.log('vote on first proposal')
  await contracts.proposals.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })

  console.log('vote on second proposal')
  await contracts.proposals.favour(seconduser, 2, 20, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(firstuser, 2, 30, { authorization: `${firstuser}@active` })

  console.log('vote on 4th proposal')
  await contracts.proposals.favour(seconduser, 4, 11, { authorization: `${seconduser}@active` })

  const supportForAlliances = await getTableRows({
    code: proposals,
    scope: "alliance",
    table: 'support',
    json: true
  })

  const supportForCampaigns = await getTableRows({
    code: proposals,
    scope: "campaign",
    table: 'support',
    json: true
  })

  // console.log("supportForCampaigns "+JSON.stringify(supportForCampaigns, null, 2))
  // console.log("supportForAlliances "+JSON.stringify(supportForAlliances, null, 2))

  console.log('execute proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  const props = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true
  })

  await contracts.proposals.initnumprop({ authorization: `${proposals}@active` })

  const testQuorum = async (numberProposals, expectedValue) => {
    try {
      await contracts.proposals.testquorum(numberProposals, { authorization: `${proposals}@active` })
    } catch (err) {
      //console.log("catch errrrrr ==== \n", JSON.stringify(err, null, 2), "\n====")
      assert({
        given: 'get quorum called',
        should: 'give the correct quorum threshold',
        expected: `assertion failure with message: ${expectedValue}`,
        actual: err.json.error.details[0].message
      })
    }
  }

  const min = 7
  const max = 40

  await testQuorum(0, min)
  await testQuorum(1, max)
  await testQuorum(2, max)
  await testQuorum(5, 20)
  await testQuorum(10, 10)
  await testQuorum(20, min)

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

  assert({
    given: 'campaign support',
    should: 'have the right numbers',
    actual: supportForCampaigns.rows[0],
    expected: {
      "propcycle": 1,
      "num_proposals": 3,
      "total_voice_cast": 60,
      "voice_needed": 20
    }
  })

  assert({
    given: 'alliance support',
    should: 'have the right numbers',
    actual: supportForAlliances.rows[0],
    expected: {
      "propcycle": 1,
      "num_proposals": 1,
      "total_voice_cast": 11,
      "voice_needed": 5
    }
  })


})

describe('Creating Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, organization })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('organization reset')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })

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

  console.log('create organization')
  const org1 = "org1"
  await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
  await contracts.organization.create(firstuser, org1, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })

  console.log('org create campaign')
  var orgCreateCampaign = false
  try {
    await contracts.proposals.createx(org1, org1, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${org1}@active` })
    orgCreateCampaign = true
  } catch (err) {

  }

  console.log('org create alliance')
  var orgCreateAlliance = false
  await contracts.proposals.createx(org1, org1, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', alliancesbank, [ 10, 30, 30, 30 ], { authorization: `${org1}@active` })
  orgCreateAlliance = true

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

  assert({
    given: 'create campaign proposal with org ',
    should: 'fail',
    actual: orgCreateCampaign,
    expected: false
  })
  
  assert({
    given: 'create alliance proposal with org ',
    should: 'succeed',
    actual: orgCreateAlliance,
    expected: true
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

  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })

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
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(thirduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(fourthuser, { authorization: `${accounts}@active` })

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
  await sleep(2000)

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

  //console.log("min stake "+JSON.stringify(minStakes, null, 2))

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

describe('Active count and vote power', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('set propmajority to 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })
  await contracts.settings.configure('batchsize', 10, { authorization: `${settings}@active` })

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

  const testActiveSize = async (active, voteP) => {
    const sizes = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'sizes',
      json: true,
    })
    const activeArray = sizes.rows.filter(r => r.id === 'user.act.sz')
    const votePowerSize = sizes.rows.filter(r => r.id === 'votepow.sz')
    const activeSize = activeArray.length > 0 ? activeArray[0].size : 0
    const votePower = votePowerSize.length > 0 ? votePowerSize[0].size : 0
    assert({
      given: 'test active size',
      should: 'match',
      expected: active,
      actual: activeSize
    })
    assert({
      given: 'vote power size',
      should: 'match',
      expected: voteP,
      actual: votePower
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

  const votePower1 = 30
  const votePower2 = 40
  const votePower3 = 90

  console.log('set contribution scores')
  await contracts.harvest.testupdatecs(firstuser, votePower1, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, votePower2, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, votePower3, { authorization: `${harvest}@active` })

  console.log('set propcyclesec to 2 seconds')
  await contracts.settings.configure('propcyclesec', 2, { authorization: `${settings}@active` })

  const now = parseInt(Date.now() / 1000)

  console.log('testActives - inital actives')
  await contracts.proposals.initsz( { authorization: `${proposals}@active` })

  await testActiveSize(3, 0)

  await sleep(1000)

  const createprop = async () => {
    console.log('create prop...')
    await contracts.proposals.createx(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
    await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '', { authorization: `${firstuser}@active` })
    await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
    await sleep(3000)
  }

  await createprop()

  console.log('vote')
  await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })
    
  console.log('onperiod 1')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  
  await testActiveSize(3, votePower3 + votePower2 + votePower1)

  console.log('sleep 2 cycle lengths + 1/2 cycle length')
  await sleep(4500)

  console.log('onperiod 2')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)


  const sizes = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'sizes',
    json: true,
  })
  const actives = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'actives',
    json: true,
  })

  console.log('sizes: '+JSON.stringify(sizes, null, 2))

  console.log('actives '+JSON.stringify(actives, null, 2))

  console.log('only 1 remains active')
  await testActiveSize(1, votePower2)

  await sleep(3000)
  
  console.log('vote on prop to become active again')
  await createprop()
  await sleep(2000)
  await contracts.proposals.favour(firstuser, 2, 5, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(seconduser, 2, 1, { authorization: `${seconduser}@active` })
  await contracts.proposals.initsz( { authorization: `${proposals}@active` })

  // Note: we don't do active size anymore.  
  //await testActiveSize(2, 40) // not sure we still even use this..

  //console.log("run props")
  //await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  //await sleep(3000)

  //await testActiveSize(2, votePower2 + votePower1)

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

  console.log('vdecayprntge = 15%')
  await contracts.settings.configure('vdecayprntge', 15, { authorization: `${settings}@active` })
  
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

    const voiceAlliance = await eos.getTableRows({
      code: proposals,
      scope: 'alliance',
      table: 'voice',
      json: true,
    })

    const voiceHypha = await eos.getTableRows({
      code: proposals,
      scope: 'hypha',
      table: 'voice',
      json: true,
    })

    assert({
      given: 'ran voice decay for the ' + n + ' time',
      should: 'decay voices if required',
      actual: voice.rows.map(r => r.balance),
      expected: expectedValues
    })
    assert({
      given: 'ran voice decay for the ' + n + ' time',
      should: 'decay voices for alliance if required',
      actual: voiceAlliance.rows.map(r => r.balance),
      expected: expectedValues
    })
    assert({
      given: 'ran voice decay for the ' + n + ' time',
      should: 'decay voices for hypha if required',
      actual: voiceHypha.rows.map(r => r.balance),
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


describe('Voice scope', async assert => {

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

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })

  console.log('create alliance proposal')
  await contracts.proposals.create(firstuser, firstuser, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, { authorization: `${firstuser}@active` })
  
  console.log('create campaign proposal')
  await contracts.proposals.create(firstuser, firstuser, '12.0000 SEEDS', 'campaign', 'test campaign', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.proposals.create(firstuser, firstuser, '12.0000 SEEDS', 'campaign', 'test campaign 2', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })

  console.log('stake')
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '2', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  
  console.log('active proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  console.log('add voice')
  await contracts.proposals.testsetvoice(firstuser, 40, { authorization: `${proposals}@active` })
  await contracts.proposals.testsetvoice(seconduser, 60, { authorization: `${proposals}@active` })

  console.log('spend voice')
  await contracts.proposals.against(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(seconduser, 2, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(seconduser, 3, 8, { authorization: `${seconduser}@active` })

  const voiceCampaignsAfter = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })  

  const voiceAlliancesAfter = await eos.getTableRows({
    code: proposals,
    scope: 'alliance',
    table: 'voice',
    json: true,
  })

  assert({
    given: 'voice for campaigns used',
    should: 'spend the correct scope',
    expected: [
      { account: 'seedsuseraaa', balance: 40 },
      { account: 'seedsuserbbb', balance: 44 }
    ],
    actual: voiceCampaignsAfter.rows
  })

  assert({
    given: 'voice for alliances used',
    should: 'spend the correct scope',
    expected: [
      { account: 'seedsuseraaa', balance: 40 },
      { account: 'seedsuserbbb', balance: 52 }
    ],
    actual: voiceAlliancesAfter.rows
  })

})

describe('delegate trust', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('configure voterep2.ind to 2')
  await contracts.settings.configure('voterep2.ind', 2, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 10, { authorization: `${settings}@active` })
  console.log('majority 80')
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })
  console.log('disable quorum')
  await contracts.settings.configure('quorum.base', 0, { authorization: `${settings}@active` })

  console.log('join users')
  const users = [firstuser, seconduser, thirduser, fourthuser, fifthuser]
  const voices = [20, 10, 50, 35, 22]
  for (let i = 0; i < users.length; i++) {
    await contracts.accounts.adduser(users[i], `user ${i}`, 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testcitizen(users[i], { authorization: `${accounts}@active` })
    await contracts.proposals.testsetvoice(users[i], voices[i], { authorization: `${proposals}@active` })
  }

  console.log('create campaign proposal')
  await contracts.proposals.create(firstuser, firstuser, '12.0000 SEEDS', 'campaign', 'test campaign', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })

  console.log('create alliance proposal')
  await contracts.proposals.create(firstuser, firstuser, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, { authorization: `${firstuser}@active` })
  
  console.log('create hypha proposal')
  await contracts.proposals.create(firstuser, hyphabank, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', milestonebank, { authorization: `${firstuser}@active` })

  console.log('stake')
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '2', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '3', { authorization: `${firstuser}@active` })
  
  console.log('active proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  for (let i = 0; i < users.length; i++) {
    await contracts.proposals.testsetvoice(users[i], voices[i], { authorization: `${proposals}@active` })
  }

  console.log('delegate trust for campaigns')
  const scopeCampaigns = proposals
  await contracts.proposals.delegate(seconduser, firstuser, scopeCampaigns, { authorization: `${seconduser}@active` })
  await contracts.proposals.delegate(thirduser, firstuser, scopeCampaigns, { authorization: `${thirduser}@active` })
  await contracts.proposals.delegate(fourthuser, thirduser, scopeCampaigns, { authorization: `${fourthuser}@active` })

  console.log('delegate trust for alliances')
  const scopeAlliance = 'alliance'
  await contracts.proposals.delegate(seconduser, firstuser, scopeAlliance, { authorization: `${seconduser}@active` })
  await contracts.proposals.delegate(seconduser, thirduser, scopeAlliance, { authorization: `${seconduser}@active` })
  await contracts.proposals.delegate(fourthuser, thirduser, scopeAlliance, { authorization: `${fourthuser}@active` })

  console.log('delegate trust for hypha')
  const scopeHypha = 'hypha'
  await contracts.proposals.delegate(seconduser, thirduser, scopeHypha, { authorization: `${seconduser}@active` })

  let avoidCyclesWorks = true
  try {
    await contracts.proposals.delegate(firstuser, fourthuser, scopeCampaigns, { authorization: `${firstuser}@active` })
    avoidCyclesWorks = false
  } catch (err) {
    console.log('no cycles allowed')
  }

  const getVoices = async () => {
    const voiceCampaigns = await eos.getTableRows({
      code: proposals,
      scope: scopeCampaigns,
      table: 'voice',
      json: true,
    })
    const voiceAlliances = await eos.getTableRows({
      code: proposals,
      scope: scopeAlliance,
      table: 'voice',
      json: true,
    })
    const voiceHypha = await eos.getTableRows({
      code: proposals,
      scope: scopeHypha,
      table: 'voice',
      json: true,
    })
    return {
      campaigns: voiceCampaigns.rows,
      alliances: voiceAlliances.rows,
      hypha: voiceHypha.rows
    }
  }

  const delegations = await eos.getTableRows({
    code: proposals,
    scope: scopeCampaigns,
    table: 'deltrusts',
    json: true,
  })

  console.log('try to vote when voice is deletegated')
  let noVoteWhenVoiceIsDelegated = true
  try {
    await contracts.proposals.favour(seconduser, 1, 5, { authorization: `${seconduser}@active` })
    noVoteWhenVoiceIsDelegated = false
  } catch (err) {
    console.log('user can not vote when voice is delegated')
  }

  console.log('vote for campaigns')
  await contracts.proposals.favour(firstuser, 1, 5, { authorization: `${firstuser}@active` })
  await sleep(5000)

  console.log('vote for alliances')
  await contracts.proposals.against(thirduser, 2, 50, { authorization: `${thirduser}@active` })
  await sleep(5000)

  await contracts.proposals.against(thirduser, 3, 50, { authorization: `${thirduser}@active` })
  await sleep(5000)

  const usersTable = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const reps = usersTable.rows.map(r => r.reputation)

  const voicesAfterVote = await getVoices()

  console.log('pass proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  for (let i = 0; i < users.length; i++) {
    await contracts.proposals.testsetvoice(users[i], voices[i], { authorization: `${proposals}@active` })
  }

  console.log('add another delegation')
  await contracts.proposals.delegate(fifthuser, firstuser, scopeCampaigns, { authorization: `${fifthuser}@active` }) 

  console.log('cancel trust delegation')
  await contracts.proposals.undelegate(fourthuser, scopeCampaigns, { authorization: `${fourthuser}@active` })
  
  console.log('change opinion')
  await contracts.proposals.against(firstuser, 1, 10, { authorization: `${firstuser}@active` })
  await sleep(3000)

  const voicesAfterOnperiod = await getVoices()

  console.log('cancel trust delegation')
  await contracts.proposals.undelegate(seconduser, scopeCampaigns, { authorization: `${firstuser}@active` })
  await contracts.proposals.undelegate(thirduser, scopeCampaigns, { authorization: `${firstuser}@active` })
  await contracts.proposals.undelegate(fifthuser, scopeCampaigns, { authorization: `${fifthuser}@active` })

  const delCampaigns = await eos.getTableRows({
    code: proposals,
    scope: scopeCampaigns,
    table: 'deltrusts',
    json: true,
  })

  assert({
    given: 'trust delegated',
    should: 'have the correct delegation entries',
    actual: delegations.rows.map(r => {
      delete r.timestamp
      return r
    }),
    expected: [
      {
        delegator: seconduser,
        delegatee: firstuser,
        weight: '1.00000000000000000'
      },
      {
        delegator: thirduser,
        delegatee: firstuser,
        weight: '1.00000000000000000'
      },
      {
        delegator: fourthuser,
        delegatee: thirduser,
        weight: '1.00000000000000000'
      }
    ]
  })

  assert({
    given: 'user delegated its voice',
    should: 'not be able to vote by itself',
    actual: noVoteWhenVoiceIsDelegated,
    expected: true
  })

  assert({
    given: 'user delegated its voice',
    should: 'give a user a percentage of the earned reputation',
    actual: reps,
    expected: [2, 1, 1, 1, 0]
  })

  assert({
    given: 'trust delegated',
    should: 'decrease the voice of delegators',
    actual: voicesAfterVote,
    expected: {
      campaigns: [
        { account: firstuser, balance: 15 },
        { account: seconduser, balance: 8 },
        { account: thirduser, balance: 38 },
        { account: fourthuser, balance: 27 },
        { account: fifthuser, balance: 22 }
      ],
      alliances: [
        { account: firstuser, balance: 20 },
        { account: seconduser, balance: 0 },
        { account: thirduser, balance: 0 },
        { account: fourthuser, balance: 0 },
        { account: fifthuser, balance: 22 }
      ],
      hypha: [
        { account: firstuser,balance:20 },
        { account: seconduser, balance: 0 },
        { account: thirduser, balance: 0 },
        { account: fourthuser, balance:35 },
        { account: fifthuser, balance:22 }
      ]
    }
  })

  assert({
    given: 'trust delegated and opinion changed',
    should: 'decrease the voice of delegators',
    actual: voicesAfterOnperiod,
    expected: {
      campaigns: [
        { account: firstuser, balance: 10 },
        { account: seconduser, balance: 5 },
        { account: thirduser, balance: 25 },
        { account: fourthuser, balance: 35 },
        { account: fifthuser, balance: 22 }
      ],
      alliances: [
        { account: firstuser, balance: 20 },
        { account: seconduser, balance: 10 },
        { account: thirduser, balance: 50 },
        { account: fourthuser, balance: 35 },
        { account: fifthuser, balance: 22 }
      ],
      hypha: [
        { account: firstuser, balance:20 },
        { account: seconduser, balance: 10 },
        { account: thirduser, balance: 50 },
        { account: fourthuser, balance: 35 },
        { account: fifthuser, balance: 22 }
      ]
    }
  })

  assert({
    given: 'trust canceled by delegatee',
    should: 'cancel trust delegation',
    actual: delCampaigns.rows,
    expected: []
  })

  assert({
    given: 'try to delegate voice',
    should: 'not create a cycle',
    actual: avoidCyclesWorks,
    expected: true
  })

})

describe("invite campaigns", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow, onboarding })

  const secondUserInitialBalance = await getBalance(seconduser)

  const checkCampaigns = async (number) => {
    const campaigns = await eos.getTableRows({
      code: onboarding,
      scope: onboarding,
      table: 'campaigns',
      json: true,
    })
    assert({
      given: 'proposals creating campaigns',
      should: 'have the correct number of them',
      actual: campaigns.rows.length,
      expected: number
    })
  }

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('prop.al.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })
  await contracts.settings.configure('quorum.base', 90, { authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('onboarding reset')
  await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

  console.log('token reset')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('update contribution score of citizens')
  await contracts.harvest.testupdatecs(firstuser, 20, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 40, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 60, { authorization: `${harvest}@active` })

  const cyclesTable = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'cycle',
    json: true
  })
  const initialCycle = cyclesTable.rows[0] ? cyclesTable.rows[0].propcycle : 0

  await contracts.proposals.createinvite(
    firstuser, firstuser, '200.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', 
    campaignbank, '20.0000 SEEDS', '6.0000 SEEDS', '2.0000 SEEDS', { authorization: `${firstuser}@active` })

  await contracts.proposals.createinvite(
    seconduser, firstuser, '300.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', 
    campaignbank, '40.0000 SEEDS', '6.0000 SEEDS', '50.0000 SEEDS', { authorization: `${seconduser}@active` })

  await contracts.proposals.createinvite(
    seconduser, firstuser, '300.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', 
    campaignbank, '40.0000 SEEDS', '6.0000 SEEDS', '50.0000 SEEDS', { authorization: `${seconduser}@active` })

  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, proposals, '500.0000 SEEDS', '2', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(seconduser, proposals, '500.0000 SEEDS', '3', { authorization: `${seconduser}@active` })
  
  console.log('move proposal to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  await sleep(3000)

  console.log('vote on proposal')
  await contracts.proposals.favour(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(seconduser, 1, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(thirduser, 1, 8, { authorization: `${thirduser}@active` })

  await contracts.proposals.favour(firstuser, 2, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.favour(seconduser, 2, 8, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(thirduser, 2, 8, { authorization: `${thirduser}@active` })
  
  console.log('approve proposal')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await checkCampaigns(2)

  const proposalsBalanceBefore = await getBalance(proposals)

  console.log('create invite')
  const inviteSecret = await ramdom64ByteHexString()
  const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')
  await contracts.onboarding.campinvite(1, firstuser, '10.0000 SEEDS', '8.0000 SEEDS', inviteHash, { authorization: `${firstuser}@active` })

  console.log('running onperiod 2')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(1000)

  console.log('running onperiod 3')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(1000)

  console.log('downvoting the first proposal')
  await contracts.proposals.against(firstuser, 1, 8, { authorization: `${firstuser}@active` })
  await contracts.proposals.against(seconduser, 1, 8, { authorization: `${seconduser}@active` })

  console.log('running on period 4')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await checkCampaigns(1)

  console.log('running on period 5')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(1000)

  console.log('running on period 6')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(1000)

  const proposalsBalanceAfter = await getBalance(proposals)

  assert({
    given: 'a proposal rejected',
    should: 'have the correct balance',
    actual: proposalsBalanceAfter - proposalsBalanceBefore,
    expected: 200
  })

  const campaigns = await eos.getTableRows({
    code: onboarding,
    scope: onboarding,
    table: 'campaigns',
    json: true,
  })
  console.log(campaigns)

  const props = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true,
  })
  console.log(props)

})

describe('Alliance campaigns', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow, onboarding, pool })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
  console.log('change min stake')
  await contracts.settings.configure('prop.cmp.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('prop.al.min', 500 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure('propmajority', 80, { authorization: `${settings}@active` })
  await contracts.settings.configure('quorum.base', 90, { authorization: `${settings}@active` })

  const eventSource = 'dao.hypha'
  const eventName = 'golive'

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('pool reset')
  await contracts.pool.reset({ authorization: `${pool}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })
  await contracts.escrow.resettrigger(eventSource, { authorization: `${escrow}@active` })

  console.log('onboarding reset')
  await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

  console.log('token reset')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('give rank in the contribution score table')
  await contracts.harvest.testupdatecs(firstuser, 99, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 99, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 99, { authorization: `${harvest}@active` })
  
  const propose = async (user, amount, propId) => {
    await contracts.proposals.create(user, user, amount, 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, { authorization: `${user}@active` })
    await contracts.token.transfer(user, proposals, '500.0000 SEEDS', `${propId}`, { authorization: `${user}@active` })
  }

  const voteProp = async (propId, approve=true) => {
    const vote = approve ? contracts.proposals.favour : contracts.proposals.against
    await vote(firstuser, propId, 8, { authorization: `${firstuser}@active` })
    await vote(seconduser, propId, 8, { authorization: `${seconduser}@active` })
    await vote(thirduser, propId, 8, { authorization: `${thirduser}@active` })
  }

  const checkEscrowLocks = async (expectedLocks) => {
    const lockTable = await eos.getTableRows({
      code: escrow,
      scope: escrow,
      table: 'locks',
      json: true,
    })
    const locks = lockTable.rows.map(r => {
      return {
        propId: parseInt(r.notes.substring(13)),
        amount: r.quantity
      }
    })
    assert({
      given: 'alliances accepted',
      should: 'have the correct entries in the locks table',
      actual: locks,
      expected: expectedLocks
    })
  }

  const checkProps = async (expectedProps) => {
    const propsTable = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'props',
      json: true,
    })
    const props = propsTable.rows.map(r => {
      return {
        propId: r.id,
        status: r.status,
        stage: r.stage
      }
    })
    assert({
      given: 'onperiod ran, props',
      should: 'have the correct status and stage',
      actual: props,
      expected: expectedProps
    })
  }

  console.log('create alliance proposal')
  const amountRequested = '12.0000 SEEDS'
  await propose(firstuser, amountRequested, 1) // passing, lock claimed successfully
  await propose(firstuser, amountRequested, 2) // not passing
  await propose(seconduser, amountRequested, 3) // passing, but downvoted
  await propose(thirduser, amountRequested, 4) // passing, but failing before claiming lock

  console.log('activate proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await voteProp(1)
  await voteProp(2, false)
  await voteProp(3)
  await voteProp(4)

  console.log('running onperiod 1')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  let expectedLocks = [
    {
      propId: 1,
      amount: amountRequested
    },
    {
      propId: 3,
      amount: amountRequested
    },
    {
      propId: 4,
      amount: amountRequested
    }
  ]

  await checkEscrowLocks(expectedLocks)

  console.log('running onperiod 2')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  console.log('downvote prop 3')
  await voteProp(3, false)

  console.log('running onperiod 3')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  expectedLocks = expectedLocks.filter(e => e.propId !== 3)
  await checkEscrowLocks(expectedLocks)

  console.log('downvote prop 4')
  await voteProp(4, false)

  console.log('going live')
  await contracts.escrow.triggertest(eventSource, eventName, 'test event', { authorization: `${escrow}@active` })
  await sleep(5000)

  const poolBalances = await eos.getTableRows({
    code: pool,
    scope: pool,
    table: 'balances',
    json: true
  })
  
  await checkEscrowLocks([
    { propId: 4, amount: '12.0000 SEEDS' }
  ])

  assert({
    given: 'golive triggered',
    should: 'have the correct pool balances',
    actual: poolBalances.rows,
    expected: [
      { account: firstuser, balance: '12.0000 SEEDS' }
    ]
  })

  await checkProps([
    {
      propId: 1,
      status: 'passed',
      stage: 'done'
    },
    {
      propId: 2,
      status: 'rejected',
      stage: 'done'
    },
    {
      propId: 3,
      status: 'rejected',
      stage: 'done'
    },
    {
      propId: 4,
      status: 'evaluate',
      stage: 'active'
    }
  ])

  console.log('running onperiod 4')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(2000)

  await checkEscrowLocks([])

  await checkProps([
    {
      propId: 1,
      status: 'passed',
      stage: 'done'
    },
    {
      propId: 2,
      status: 'rejected',
      stage: 'done'
    },
    {
      propId: 3,
      status: 'rejected',
      stage: 'done'
    },
    {
      propId: 4,
      status: 'rejected',
      stage: 'done'
    }
  ])

})


describe('fix alliances', async assert => {
 
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, settings, escrow })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('escrow reset')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  const printProposals = async () => {
    const proposalsTable = await eos.getTableRows({
      code: proposals,
      scope: proposals,
      table: 'props',
      json: true
    })
    console.log(proposalsTable)
    const escrowTable = await eos.getTableRows({
      code: escrow,
      scope: escrow,
      table: 'locks',
      json: true
    })
    console.log(escrowTable)
  }

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })

  console.log('create alliances')
  await contracts.proposals.testalliance(1, firstuser, '10.0000 SEEDS', '1.0000 SEEDS', 'evaluate', 'active', 'alliance', { authorization: `${proposals}@active` })
  await contracts.proposals.testalliance(2, seconduser, '10.0000 SEEDS', '1.0000 SEEDS', 'passed', 'done', 'alliance', { authorization: `${proposals}@active` })
  await contracts.proposals.testalliance(3, thirduser, '10.0000 SEEDS', '1.0000 SEEDS', 'rejected', 'done', 'alliance', { authorization: `${proposals}@active` })
  await contracts.proposals.testalliance(4, thirduser, '10.0000 SEEDS', '0.0000 SEEDS', 'open', 'active', 'alliance', { authorization: `${proposals}@active` })
  await contracts.proposals.testalliance(5, firstuser, '10.0000 SEEDS', '1.0000 SEEDS', 'evaluate', 'active', 'cmp.invite', { authorization: `${proposals}@active` })

  await printProposals()

  await contracts.proposals.migalliances(0, 2, { authorization: `${proposals}@active` })
  await sleep(2000)

  await printProposals()

  const locksTable = await eos.getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })

  assert({
    given: 'alliances not being paid out',
    should: 'send the missing amount to escrow',
    actual: locksTable.rows.length,
    expected: 2
  })


})


describe('Hypha proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow, onboarding, pool })

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('harvest reset')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('proposals reset')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })

  console.log('create proposal '+milestonebank)

  await contracts.proposals.createx(firstuser, hyphabank, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', milestonebank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })
  await contracts.proposals.createx(firstuser, hyphabank, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', milestonebank, [ 10, 30, 30, 30 ], { authorization: `${firstuser}@active` })

  await contracts.proposals.createx(seconduser, seconduser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, [ 10, 30, 30, 30 ], { authorization: `${seconduser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '555.0000 SEEDS', '2', { authorization: `${firstuser}@active` })

  console.log('deposit stake')
  await contracts.token.transfer(seconduser, proposals, '555.0000 SEEDS', '', { authorization: `${seconduser}@active` })

  console.log('update contribution score of citizens')
  await contracts.harvest.testupdatecs(firstuser, 90, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 90, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 90, { authorization: `${harvest}@active` })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  await contracts.proposals.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })
  await contracts.proposals.favour(thirduser, 1, 10, { authorization: `${thirduser}@active` })

  await contracts.proposals.favour(thirduser, 2, 1, { authorization: `${thirduser}@active` })

  const voiceTable = await eos.getTableRows({
    code: proposals,
    scope: 'hypha',
    table: 'voice',
    json: true
  })
  console.log(voiceTable)

  assert({
    given: 'voted on hypha props',
    should: 'have the correct voice',
    actual: voiceTable.rows,
    expected: [
      { account: firstuser, balance: 90 },
      { account: seconduser, balance: 80 },
      { account: thirduser, balance: 79 }
    ]
  })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
  await sleep(3000)

  const props = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    json: true,
  })

  assert({
    given: 'hypha props created',
    should: 'evaluated them correctly',
    actual: props.rows.map(p => {
      return {
        id: p.id,
        status: p.status,
        stage: p.stage,
        current_payout: p.current_payout,
        campaign_type: p.campaign_type
      }
    }),
    expected: [
      {
        id: 1,
        status: 'passed',
        stage: 'done',
        current_payout: '100.0000 SEEDS',
        campaign_type: 'milestone'
      },
      {
        id: 2,
        status: 'rejected',
        stage: 'done',
        current_payout: '0.0000 SEEDS',
        campaign_type: 'milestone'
      },
      {
        id: 3,
        status: 'rejected',
        stage: 'done',
        current_payout: '0.0000 SEEDS',
        campaign_type: 'cmp.funding'
      }
    ]
  })

  const supportTable = await eos.getTableRows({
    code: proposals,
    scope: 'hypha',
    table: 'support',
    json: true
  })

  assert({
    given: 'milestone props created',
    should: 'have the correct support',
    actual: supportTable.rows,
    expected: [
      {
        propcycle: 1,
        num_proposals: 2,
        total_voice_cast: 21,
        voice_needed: 9
      },
      {
        propcycle: 2,
        num_proposals: 0,
        total_voice_cast: 0,
        voice_needed: 0
      }
    ]
  })

})
