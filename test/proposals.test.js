const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts, isLocal } = require('../scripts/helper')

const { harvest, accounts, proposals, settings, escrow, token, campaignbank, milestonebank, alliancesbank, firstuser, seconduser, thirduser, fourthuser } = names

describe('Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

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

  await contracts.proposals.create(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.proposals.create(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })

  console.log('create another proposal')
  await contracts.proposals.create(seconduser, seconduser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${seconduser}@active` })

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
  await contracts.proposals.create(firstuser, firstuser, '200.0000 SEEDS', 'prop to cancel', 'will be canceled', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, proposals, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  
  const numberOfProposalsBeforeCancel = await numberOfProposals()

  const balanceBeforeCancel = await getBalance(firstuser)
  await contracts.proposals.cancel(4, { authorization: `${firstuser}@active` })
  const balanceAfterCancel = await getBalance(firstuser)

  const numberOfProposalsAfterCancel = await numberOfProposals()

  console.log('create alliance proposal')
  await contracts.proposals.create(fourthuser, fourthuser, '12.0000 SEEDS', 'alliance', 'test alliance', 'description', 'image', 'url', alliancesbank, { authorization: `${fourthuser}@active` })

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

  console.log('update contribution score of citizens')
  await contracts.harvest.testupdatecs(firstuser, 20, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(seconduser, 40, { authorization: `${harvest}@active` })
  await contracts.harvest.testupdatecs(thirduser, 60, { authorization: `${harvest}@active` })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

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

  console.log('execute proposals')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

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
    expected: 600
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
    should: 'show passed proposal',
    actual: rows[0],
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
      stage: 'done',
      title: 'title2',
      summary: 'summary2',
      description: 'description2',
      image: 'image2',
      url: 'url2',
      status: 'passed',
      fund: campaignbank,
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
    }
  })

  assert({
    given: 'executed proposal',
    should: 'decrease bank balance',
    actual: balancesAfter[2] - balancesBefore[2],
    expected: -100
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
      "quantity": "12.0000 SEEDS",
      "trigger_event": "golive",
      "trigger_source": "dao.hypha",
      "notes": "proposal id: 4",
    }
  })

})

describe('Neutral Vote', async assert => {
  
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, proposals, token, harvest, settings, escrow })

  const secondUserInitialBalance = await getBalance(seconduser)

  console.log('settings reset')
  await contracts.settings.reset({ authorization: `${settings}@active` })

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

  console.log('create proposal '+campaignbank)
  await contracts.proposals.create(firstuser, firstuser, '100.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  // await contracts.proposals.create(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  // console.log('deposit stake (memo 2)')
  // await contracts.token.transfer(firstuser, proposals, '500.0000 SEEDS', '2', { authorization: `${firstuser}@active` })

  console.log('add voice')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })

  console.log('force status')
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })

  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  const voiceBefore = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    json: true,
  })

  console.log('favour first proposal')
  await contracts.proposals.neutral(seconduser, 1, { authorization: `${seconduser}@active` })
  await contracts.proposals.against(firstuser, 1, 8, { authorization: `${firstuser}@active` })

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

  assert({
    given: 'voice before voting',
    should: 'have all voice points',
    actual: voiceBefore.rows,
    expected: [
      { account: firstuser, balance: 44 },
      { account: seconduser, balance: 44 }
    ]
  })

  assert({
    given: 'voice after voting',
    should: 'not decrease voice if voted for noopinion',
    actual: voiceAfter.rows,
    expected: [
      { account: firstuser, balance: 36 },
      { account: seconduser, balance: 44 }
    ]
  })

  assert({
    given: 'voted for a proposal',
    should: 'have entry in votes table',
    actual: votes.rows,
    expected: [
      {
        proposal_id: 1,
        account: firstuser,
        amount: 8,
        vote: 'distrust'
      },
      {
        proposal_id: 1,
        account: seconduser,
        amount: 0,
        vote: 'abstain'
      }
    ]
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

  await contracts.proposals.changetrust(firstuser, 1, { authorization: `${proposals}@active` })

  await check("after", "has voice", 1) 

  

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
  await contracts.settings.configure("propminstake", 2 * 10000, { authorization: `${settings}@active` })

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

  console.log('create proposal')
  await contracts.proposals.create(firstuser, firstuser, '2.0000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.proposals.create(firstuser, firstuser, '2.5000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
  await contracts.proposals.create(seconduser, seconduser, '1.4000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${seconduser}@active` })

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(firstuser, proposals, '2.0000 SEEDS', '2', { authorization: `${firstuser}@active` })
  console.log('deposit stake 3')
  await contracts.token.transfer(seconduser, proposals, '2.0000 SEEDS', '3', { authorization: `${seconduser}@active` })
  console.log('move proposals to active')
  await contracts.proposals.onperiod({ authorization: `${proposals}@active` })

  let users = [firstuser, seconduser, thirduser, fourthuser]
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

  //console.log("props "+JSON.stringify(props, null, 2))

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
    expected: "passed"
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
    await contracts.proposals.create(firstuser, "23", '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
    createdProp = true
    console.log('error')
  } catch (err) {

  }

  console.log('create proposal with invalid fund')
  var createdFundProp = false
  try {
    await contracts.proposals.create(firstuser, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', "clowns", { authorization: `${firstuser}@active` })
    console.log('error')
    createdFundProp = true
  } catch (err) {

  }

  console.log('create proposal with non seeds user')
  var createNonSeedsUser = false
  try {
    await contracts.proposals.create(milestonebank, firstuser, '55.7000 SEEDS', 'title', 'summary', 'description', 'image', 'url', campaignbank, { authorization: `${firstuser}@active` })
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

