const { describe } = require('riteway')
const { eos, names, isLocal, initContracts, activePublicKey, getBalance, getBalanceFloat,
  ramdom64ByteHexString, sha256, fromHexString, getTableRows } = require('../scripts/helper')
const { equals } = require('ramda')

const publicKey = 'EOS7iYzR2MmQnGga7iD2rPzvm5mEFXx6L1pjFTQYKRtdfDcG9NTTU'

const { accounts, proposals, harvest, token, settings, history, exchange, organization, onboarding, escrow, firstuser, seconduser, thirduser, fourthuser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// helper function
const get_reps = async () => {
  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    json: true,
  })

  const result = users.rows.map( ({ rep }) => rep )

  return result
}

const can_vote = async (user) => {
  const voice = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    lower_bound: user,
    upper_bound: user,
    json: true,
  })

  return voice.rows.length == 1
}
const setting_in_seeds = async (key) => {
  const value = await eos.getTableRows({
    code: settings,
    scope: settings,
    table: 'config',
    lower_bound: key,
    upper_bound: key,
    json: true,
  })
  return Math.round(value.rows[0].value / 10000) // not entirely correct but works for now
}

const get_settings = async (key) => {
  const value = await eos.getTableRows({
    code: settings,
    scope: settings,
    table: 'config',
    lower_bound: key,
    upper_bound: key,
    json: true,
  })
  return value.rows[0].value
}

describe('accounts', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(accounts)
  const proposalsContract = await eos.contract(proposals)
  const thetoken = await eos.contract(token)
  const settingscontract = await eos.contract(settings)
  const escrowContract = await eos.contract(escrow)
  const exchangecontract = await eos.contract(exchange)
 
  console.log('reset proposals')
  await proposalsContract.reset({ authorization: `${proposals}@active` })

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await thetoken.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await settingscontract.reset({ authorization: `${settings}@active` })

  console.log('reset exchange')
  await exchangecontract.reset({ authorization: `${exchange}@active` })
  await exchangecontract.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

  console.log('reset escrow')
  await escrowContract.reset({ authorization: `${escrow}@active` })


  console.log('add users')
  await contract.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(thirduser, 'Third user', "individual", { authorization: `${accounts}@active` })

  console.log("filling account with Seedds for bonuses [Change this]")
  await thetoken.transfer(firstuser, accounts, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('plant 50 seeds')
  await thetoken.transfer(firstuser, harvest, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log("plant 100 seeds")
  await thetoken.transfer(firstuser, harvest, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })

  console.log('add referral firstuser is referrer for seconduser')
  await contract.addref(firstuser, seconduser, { authorization: `${accounts}@api` })

  console.log('update reputation')
  await contract.addrep(firstuser, 100, { authorization: `${accounts}@api` })
  await contract.subrep(seconduser, 1, { authorization: `${accounts}@api` })

  console.log('update user data')
  await contract.update(
    firstuser, 
    "individual", 
    "Ricky G",
    "https://m.media-amazon.com/images/M/MV5BMjQzOTEzMTk1M15BMl5BanBnXkFtZTgwODI1Mzc0MDI@._V1_.jpg",
    "I'm from the UK",
    "Roless... hmmm... ",
    "Skills: Making jokes. Some acting. Offending people.",
    "Animals",
    { authorization: `${firstuser}@active` })


  try {
    console.log('make resident')

    await contract.makeresident(firstuser, { authorization: `${firstuser}@active` })

    console.log('make citizen')
    
    await contract.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    console.log('user not ready to become citizen' + err)
  }

  console.log('test citizen')

  assert({
    given: 'not citizen',
    should: 'cant vote',
    actual: await can_vote(firstuser),
    expected: false
  })

  await contract.testcitizen(firstuser, { authorization: `${accounts}@active` })

  assert({
    given: 'citizen',
    should: 'can vote',
    actual: await can_vote(firstuser),
    expected: true
  })
  
  console.log('test resident')

  await contract.testresident(seconduser, { authorization: `${accounts}@active` })

  assert({
    given: 'resident',
    should: 'cant vote',
    actual: await can_vote(seconduser),
    expected: false
  })

  console.log('test citizen again')

  let balanceBeforeResident = await getBalance(firstuser)
  //console.log('balanceBeforeResident '+balanceBeforeResident)

  console.log('test resident')

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const refs = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'refs',
    json: true
  })

  console.log('test citizen second user')
  await contract.testcitizen(seconduser, { authorization: `${accounts}@active` })

  console.log('test testremove')
  await contract.testremove(seconduser, { authorization: `${accounts}@active` })

  const usersAfterRemove = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
  })

  const now = new Date() / 1000

  const firstTimestamp = users.rows[0].timestamp

  let factor = 100


  const checkEscrow = async (text, id, user, settingname) => {

    const amount = await setting_in_seeds(settingname)

    const escrows = await getTableRows({
      code: escrow,
      scope: escrow,
      table: 'locks',
      lower_bound: id,
      upper_bound: id,
      json: true
    })

    let n = escrows.rows.length
    let item = escrows.rows[n-1]

    delete item.id
    delete item.vesting_date
    delete item.notes
    delete item.created_date
    delete item.updated_date

    assert({
      given: text,
      should: 'reveive Seeds in escrow',
      actual: item,
      expected: 
        {
          "lock_type": "event",
          "sponsor": "refer.seeds",
          "beneficiary": user,
          "quantity": amount + ".0000 SEEDS",
          "trigger_event": "golive",
          "trigger_source": "dao.hypha",
        },
    })
  }

  await checkEscrow("referred became resident", 0, firstuser, "refrwd1.ind")
  await checkEscrow("referred became citizen", 1, firstuser, "refrwd2.ind")

  assert({
    given: 'changed reputation',
    should: 'have correct values',
    actual: users.rows.map(({ reputation }) => reputation),
    expected: [101, 0, 0]
  })

  assert({
    given: 'created user',
    should: 'have correct timestamp',
    actual: Math.abs(firstTimestamp - now) < 500,
    expected: true
  })

  assert({
    given: 'invited user',
    should: 'have row in table',
    actual: refs.rows[0],
    expected: {
      referrer: firstuser,
      invited: seconduser
    }
  })

  assert({
    given: 'users table',
    should: 'show joined users',
    actual: users.rows.map(({ account, status, nickname, reputation }) => ({ account, status, nickname, reputation })),
    expected: [{
      account: firstuser,
      status: 'citizen',
      nickname: 'Ricky G',
      reputation: 101,
    }, {
      account: seconduser,
      status: 'resident',
      nickname: 'Second user',
      reputation: 0
    },
     { "account":"seedsuserccc","status":"visitor","nickname":"Third user","reputation":0 }
    ]
  })

  assert({
    given: 'test-removed user',
    should: 'have 1 fewer users than before',
    actual: usersAfterRemove.rows.length,
    expected: users.rows.length - 1
  })

})


describe('vouching', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const checkReps = async (expectedReps, given, should) => {
  
    assert({
      given: given,
      should: should,
      actual: await get_reps(),
      expected: expectedReps
    })
  
  }
  
  const contract = await eos.contract(accounts)
  const thetoken = await eos.contract(token)
  const harvestContract = await eos.contract(harvest)
  const settingscontract = await eos.contract(settings)
  const exchangecontract = await eos.contract(exchange)

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('reset harvest')
  await harvestContract.reset({ authorization: `${harvest}@active` })

  console.log('reset token stats')
  await thetoken.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await settingscontract.reset({ authorization: `${settings}@active` })

  console.log('reset exchange')
  await exchangecontract.reset({ authorization: `${exchange}@active` })
  await exchangecontract.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

  console.log('add users')
  await contract.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(thirduser, 'Third user', "individual", { authorization: `${accounts}@active` })

  await contract.testsetrs(firstuser, 50, { authorization: `${accounts}@active` })
  await contract.testsetrs(seconduser, 50, { authorization: `${accounts}@active` })
  await contract.testsetrs(thirduser, 50, { authorization: `${accounts}@active` })
  // not yet active
  //console.log('unrequested vouch for user')
  //await contract.vouch(seconduser, thirduser, { authorization: `${seconduser}@active` })

  console.log('test citizen')
  await contract.testcitizen(firstuser, { authorization: `${accounts}@active` })

  console.log('request vouch from user')
  await contract.requestvouch(thirduser, firstuser,{ authorization: `${thirduser}@active` })
  await contract.requestvouch(seconduser, firstuser,{ authorization: `${seconduser}@active` })

  await checkReps([0, 0, 0], "init", "be empty XX")

  console.log('vouch for user')
  await contract.vouch(firstuser, seconduser, { authorization: `${firstuser}@active` })
  await contract.vouch(firstuser, thirduser, { authorization: `${firstuser}@active` })

  await checkReps([0, 20, 20], "after vouching", "get rep bonus for being vouched")

  var cantVouchTwice = true
  try {
    await contract.vouch(firstuser, seconduser, { authorization: `${firstuser}@active` })
    cantVouchTwice = false
  } catch (err) {}

  console.log('vouch for user but not resident or citizen')
  var visitorCannotVouch = true
  try {
    await contract.vouch(seconduser, thirduser,{ authorization: `${seconduser}@active` })
    visitorCannotVouch = false
  } catch (err) {}

  console.log('test resident')
  await contract.testresident(seconduser, { authorization: `${accounts}@active` })
  await checkReps([1, 20, 20], "after user is resident", "sponsor gets rep bonus for sponsoring resident")

  await contract.vouch(seconduser, thirduser,{ authorization: `${seconduser}@active` })
  await checkReps([1, 20, 30], "resident vouch", "rep bonus")

  await contract.testresident(thirduser, { authorization: `${accounts}@active` })
  await checkReps([2, 21, 30], "after user is resident", "all sponsors gets rep bonus")

  await contract.testcitizen(thirduser, { authorization: `${accounts}@active` })
  await checkReps([3, 22, 30], "after user is citizen", "all sponsors gets rep bonus")

  console.log("set max vouch to 3")
  await settingscontract.configure("maxvouch", 3, { authorization: `${settings}@active` })
  await contract.adduser(fourthuser, 'Fourth user', "individual", { authorization: `${accounts}@active` })
  await contract.vouch(firstuser, fourthuser,{ authorization: `${firstuser}@active` })
  await checkReps([3, 22, 30, 3], "max vouch reached", "can still vouch")
  let maxVouchExceeded = false
  try {
    await contract.vouch(thirduser, fourthuser,{ authorization: `${thirduser}@active` })
    maxVouchExceeded = true
  } catch (err) {
    console.log("err expected "+err)
  }


  assert({
    given: 'vouch sponsor is not resident or citizen',
    should: 'not be able to vouch',
    actual: visitorCannotVouch,
    expected: true
  })

  assert({
    given: 'vouch a second time',
    should: 'not be able to vouch',
    actual: cantVouchTwice,
    expected: true
  })
  assert({
    given: 'vouch past max points',
    should: 'not be able to vouch',
    actual: maxVouchExceeded,
    expected: false
  })



})

describe('vouching with reputation', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const checkReps = async (expectedReps, given, should) => {
    
    assert({
      given: given,
      should: should,
      actual: await get_reps(),
      expected: expectedReps
    })
  
  }
  
  const contract = await eos.contract(accounts)
  const harvestContract = await eos.contract(harvest)
  const settingscontract = await eos.contract(settings)
  const exchangecontract = await eos.contract(exchange)

  console.log('reset accounts')
  await contract.reset({ authorization: `${accounts}@active` })

  console.log('reset harvest')
  await harvestContract.reset({ authorization: `${harvest}@active` })

  console.log('reset settings')
  await settingscontract.reset({ authorization: `${settings}@active` })

  console.log('reset exchange')
  await exchangecontract.reset({ authorization: `${exchange}@active` })
  await exchangecontract.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

  console.log('add users')
  await contract.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(thirduser, 'Third user', "individual", { authorization: `${accounts}@active` })
  await contract.adduser(fourthuser, '4th user', "individual", { authorization: `${accounts}@active` })

  console.log('test citizen')
  await contract.testcitizen(firstuser, { authorization: `${accounts}@active` })

  await checkReps([], "init", "be empty")

  console.log('vouch for user')
  await contract.testsetrs(firstuser, 25, { authorization: `${accounts}@active` })
  await contract.vouch(firstuser, seconduser, { authorization: `${firstuser}@active` })

  await contract.testsetrs(firstuser, 75, { authorization: `${accounts}@active` })
  await contract.vouch(firstuser, thirduser, { authorization: `${firstuser}@active` })

  await contract.testsetrs(firstuser, 99, { authorization: `${accounts}@active` })
  await contract.vouch(firstuser, fourthuser, { authorization: `${firstuser}@active` })

  await checkReps([0, 10, 30, 40], "after vouching", "get rep bonus for being vouched")

})

describe('Ambassador and Org rewards', async assert => {

  console.log('Ambassador and Org rewards =====')

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ settings, accounts, organization, token, onboarding, escrow, exchange })

  console.log('reset contracts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  await contracts.settings.reset({ authorization: `${settings}@active` })
  await contracts.organization.reset({ authorization: `${organization}@active` })
  await contracts.escrow.reset({ authorization: `${escrow}@active` })
  await contracts.exchange.reset({ authorization: `${exchange}@active` })
  await contracts.exchange.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

  console.log('add user')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  let ambassador = firstuser
  let orgowner = "testorgowner"
  let orgaccount = "testorg11111"

  console.log('ambassador invites a user named testorgowner')
  let secret = await invite(ambassador, 800, false)

  console.log('user accepts the invite and becomes a Seeds user')
  await accept(orgowner, secret, activePublicKey, contracts)

  console.log("user sends funds to orgs.seeds contract")
  await contracts.token.transfer(orgowner, organization, "200.0000 SEEDS", "memo", { authorization: `${orgowner}@active` })

  console.log("user creates org")
  await contracts.organization.create(orgowner, orgaccount, "Org Number 1", activePublicKey, { authorization: `${orgowner}@active` })

  console.log("user sends funds to newly created org")
  await contracts.token.transfer(firstuser, orgaccount, "900.0000 SEEDS", "memo", { authorization: `${firstuser}@active` })

  console.log("org signs up people")
  let orguser1 = "seedsorgusr1"
  let orguser2 = "seedsorgusr2"

  console.log('org invites 2 users')
  let secret1 = await invite(orgaccount, 50, false)
  let secret2 = await invite(orgaccount, 100, false)

  await accept(orguser1, secret1, activePublicKey, contracts)
  await accept(orguser2, secret2, activePublicKey, contracts)

  const checkBalances = async (text, ambassadorSettingName, orgSettingName) => {

    const ambassadorReward = await setting_in_seeds(ambassadorSettingName)
    const orgReward = await setting_in_seeds(orgSettingName)

    const escrows = await getTableRows({
      code: escrow,
      scope: escrow,
      table: 'locks',
      json: true
    })
    //console.log("escrow: "+JSON.stringify(escrows, null, 2))

    let n = escrows.rows.length
    let lastTwoEscrows = [ escrows.rows[n-2], escrows.rows[n-1] ]

    lastTwoEscrows.forEach( (item) => {
      delete item.id
      delete item.vesting_date
      delete item.notes
      delete item.created_date
      delete item.updated_date
    })

    assert({
      given: text,
      should: 'reveive Seeds in escrow',
      actual: lastTwoEscrows,
      expected: [
        {
          "lock_type": "event",
          "sponsor": "refer.seeds",
          "beneficiary": "testorg11111",
          "quantity": orgReward + ".0000 SEEDS",
          "trigger_event": "golive",
          "trigger_source": "dao.hypha",
        },
        {
          "lock_type": "event",
          "sponsor": "refer.seeds",
          "beneficiary": "seedsuseraaa",
          "quantity": ambassadorReward+".0000 SEEDS",
          "trigger_event": "golive",
          "trigger_source": "dao.hypha",
        }

      ]
        
      
    })
  }

  console.log("user 1 becomes resident")
  await contracts.accounts.testresident(orguser1, { authorization: `${accounts}@active` })

  await checkBalances("after resident", "refrwd1.amb", "refrwd1.org")

  console.log("user 2 becomes citizen")
  await contracts.accounts.testcitizen(orguser2, { authorization: `${accounts}@active` })

  await checkBalances("after citizen 1", "refrwd2.amb", "refrwd2.org")

  console.log("user 1 becomes citizen")
  await contracts.accounts.testcitizen(orguser1, { authorization: `${accounts}@active` })

  await checkBalances("after citizen 2", "refrwd2.amb", "refrwd2.org")


})


describe('Proportional rewards', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ settings, accounts, organization, exchange, token, onboarding, escrow, history })

  console.log('reset contracts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  await contracts.settings.reset({ authorization: `${settings}@active` })
  await contracts.organization.reset({ authorization: `${organization}@active` })
  await contracts.escrow.reset({ authorization: `${escrow}@active` })
  await contracts.exchange.reset({ authorization: `${exchange}@active` })
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })

  console.log("set exchange price")
  await contracts.exchange.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })
  await contracts.exchange.incprice({ authorization: `${exchange}@active` })

  console.log('add user')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  let individual = firstuser
  let invited = "invited"

  console.log('individual invites a user named invited')
  let secret = await invite(individual, 800, false)

  console.log('user accepts the invite and becomes a Seeds user')
  await accept(invited, secret, activePublicKey, contracts)

  const checkBalances = async (text, amount) => {

    const escrows = await getTableRows({
      code: escrow,
      scope: escrow,
      table: 'locks',
      json: true
    })
    //console.log("escrow: "+JSON.stringify(escrows, null, 2))

    let n = escrows.rows.length
    let item = escrows.rows[n-1]

    delete item.id
    delete item.vesting_date
    delete item.notes
    delete item.created_date
    delete item.updated_date

    assert({
      given: text,
      should: 'receive Seeds in escrow',
      actual: escrows.rows[n-1],
      expected:
        {
          "lock_type": "event",
          "sponsor": "refer.seeds",
          "beneficiary": "seedsuseraaa",
          "quantity": amount+" SEEDS",
          "trigger_event": "golive",
          "trigger_source": "dao.hypha",
        }
    })
  }

  console.log("change settings")
  await contracts.settings.configure("refrwd1.ind", 3000*10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("minrwd1.ind", 20*10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("decrwd1.ind", 1, { authorization: `${settings}@active` })

  await contracts.settings.configure("refrwd2.ind", 3000*10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("minrwd2.ind", 20*10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("decrwd2.ind", 1, { authorization: `${settings}@active` })

  console.log("user becomes resident")
  await contracts.accounts.testresident(invited, { authorization: `${accounts}@active` })
  const expected_reward1 = 1116.2807 // calculated using the decay formula

  await checkBalances("after resident", expected_reward1.toFixed(4))

  console.log("user becomes citizen")
  await contracts.accounts.testcitizen(invited, { authorization: `${accounts}@active` })
  const expected_reward2 = 1116.2807 // calculated using the decay formula

  await checkBalances("after citizen", expected_reward2.toFixed(4))

})

const userStatus = async (name) => {
  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    lower_bound: name,
    upper_bound: name,
    table: 'users',
    json: true,
  })

  return users.rows[0].status

}

describe('make resident', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, token, history, exchange })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset history')
  await contracts.history.reset(firstuser, { authorization: `${history}@active` })

  console.log('reset exchange')
  await contracts.exchange.reset({ authorization: `${exchange}@active` })
  await contracts.exchange.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('add users')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })

  console.log('make resident - fail')
  try {
    await contracts.accounts.makeresident(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    //console.log('expected error' + err)
  }

  console.log('can resident - fail')
  let canresident = true
  try {
    await contracts.accounts.canresident(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    canresident = false
    //console.log('expected error' + err)
  }

  // 1 CHECK STATUS - fail
  assert({
    given: 'does not fulfill criteria for resident',
    should: 'be visitor',
    actual: await userStatus(firstuser),
    expected: 'visitor'
  })

  assert({
    given: 'does not fulfill criteria for resident - canresident is false',
    should: 'be visitor',
    actual: canresident,
    expected: false
  })

  // 2 DO SHIT
  console.log('plant 50 seeds')
  await contracts.token.transfer(firstuser, harvest, '50.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  console.log('make 10 transactions')
  for (var i=0; i<10; i++) {
    await contracts.token.transfer(firstuser, seconduser, '1.0000 SEEDS', 'memo'+i, { authorization: `${firstuser}@active` })
  }

  console.log('add referral')
  await contracts.accounts.addref(firstuser, seconduser, { authorization: `${accounts}@api` })
  console.log('update reputation')
  await contracts.accounts.addrep(firstuser, 50, { authorization: `${accounts}@api` })

  // 3 CHECK STATUS - succeed
  console.log('can resident')
  await contracts.accounts.canresident(firstuser, { authorization: `${firstuser}@active` })
  
  console.log('make resident')
  await contracts.accounts.makeresident(firstuser, { authorization: `${firstuser}@active` })

  assert({
    given: 'fulfills criteria for resident',
    should: 'is resident',
    actual: await userStatus(firstuser),
    expected: 'resident'
  })

})

describe('make citizen', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, settings, token, harvest })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('add users')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, '3 user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, '4 user', "individual", { authorization: `${accounts}@active` })

  console.log('make citizen - fail')
  try {
    await contracts.accounts.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    //console.log('expected error' + err)
  }

  console.log('can citizen - fail')
  var cancitizen = true
  try {
    await contracts.accounts.cancitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    cancitizen = false
    //console.log('expected error' + err)
  }

  assert({
    given: 'does not fulfill criteria for citizen',
    should: 'be visitor',
    actual: await userStatus(firstuser),
    expected: 'visitor'
  })

  assert({
    given: 'does not fulfill criteria for citizen - cancitizen false ',
    should: 'be false',
    actual: cancitizen,
    expected: false
  })

  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })

  console.log('make citizen - fail')
  try {
    await contracts.accounts.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    //console.log('expected error' + err)
  }

  // 1 CHECK STATUS - fail
  assert({
    given: 'does not fulfill criteria for citizen',
    should: 'be resident',
    actual: await userStatus(firstuser),
    expected: 'resident'
  })

  // 2 DO SHIT
  console.log('plant 200 seeds')
  await contracts.token.transfer(firstuser, harvest, '200.0000 SEEDS', '', { authorization: `${firstuser}@active` })
  console.log('make 50 transaction')
  for (var i=0; i<25; i++) {
    await contracts.token.transfer(firstuser, seconduser, '1.0000 SEEDS', 'memo'+i, { authorization: `${firstuser}@active` })
    await contracts.token.transfer(firstuser, thirduser, '1.0000 SEEDS', 'memo2'+i, { authorization: `${firstuser}@active` })
  }
  console.log('add 3 referrals')
  await contracts.accounts.addref(firstuser, seconduser, { authorization: `${accounts}@api` })
  await contracts.accounts.addref(firstuser, thirduser, { authorization: `${accounts}@api` })
  await contracts.accounts.addref(firstuser, fourthuser, { authorization: `${accounts}@api` })
  console.log('update reputation SCORE')
  await contracts.accounts.testsetrs(firstuser, 51, { authorization: `${accounts}@active` })

  // 3 CHECK STATUS - succeed
  try {
    await contracts.accounts.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
  }

  assert({
    given: 'does not fulfill criteria for citizen',
    should: 'is resident still',
    actual: await userStatus(firstuser),
    expected: 'resident'
  })

  await contracts.accounts.testresident(seconduser, { authorization: `${accounts}@active` })
  
  try {
    await contracts.accounts.makecitizen(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
  }
  assert({
    given: 'does not fulfill criteria for citizen',
    should: 'is resident still',
    actual: await userStatus(firstuser),
    expected: 'resident'
  })
  await contracts.settings.configure("cit.age", 0, { authorization: `${settings}@active` })

  const bal = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    lower_bound: firstuser,
    upper_bound: firstuser,
    table: 'balances',
    json: true,
  })

  console.log("can citizen - should succeed")
  await contracts.accounts.cancitizen(firstuser, { authorization: `${firstuser}@active` })

  console.log("make citizen")
  await contracts.accounts.makecitizen(firstuser, { authorization: `${firstuser}@active` })



  assert({
    given: 'does fulfill criteria for citizen',
    should: 'is citizen',
    actual: await userStatus(firstuser),
    expected: 'citizen'
  })


})

describe('reputation & cbs ranking', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('add users')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, '3 user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, '4 user', "individual", { authorization: `${accounts}@active` })

  await contracts.accounts.addrep(firstuser, 100, { authorization: `${accounts}@api` })
  await contracts.accounts.addrep(seconduser, 4, { authorization: `${accounts}@api` })
  await contracts.accounts.addrep(thirduser, 2, { authorization: `${accounts}@api` })

  await contracts.accounts.testsetcbs(firstuser, 10, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetcbs(seconduser, 20, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetcbs(thirduser, 30, { authorization: `${accounts}@active` })
  await contracts.accounts.testsetcbs(fourthuser, 40, { authorization: `${accounts}@active` })

  const reps = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    json: true
  })

  const cbs = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })

  const sizes = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'sizes',
    lower_bound: 'rep.sz',
    upper_bound: 'rep.sz',
    json: true
  })

  const sizesCBS = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'sizes',
    lower_bound: 'cbs.sz',
    upper_bound: 'cbs.sz',
    json: true
  })

  const userSize = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'sizes',
    lower_bound: 'users.sz',
    upper_bound: 'users.sz',
    json: true
  })

  await contracts.accounts.subrep(firstuser, 2, { authorization: `${accounts}@api` })
  await contracts.accounts.subrep(thirduser, 2, { authorization: `${accounts}@api` })

  await contracts.accounts.rankreps({ authorization: `${accounts}@active` })

  await contracts.accounts.rankrep(0, 0, 200, { authorization: `${accounts}@active` })

  await contracts.accounts.rankcbs(0, 0, 1, { authorization: `${accounts}@active` })
  await sleep(4000)

  const repsAfter = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    json: true
  })
  
  const cbsAfter = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })
  
  await contracts.accounts.rankcbs(0, 0, 40, { authorization: `${accounts}@active` })

  const cbsAfter2 = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })

  //console.log("reps "+JSON.stringify(repsAfter, null, 2))

  //console.log("cbs "+JSON.stringify(cbsAfter, null, 2))


  const repsAfterFirstUser = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'rep',
    lower_bound: firstuser,
    upper_bound: firstuser,
    json: true
  })

  const sizesAfter = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'sizes',
    lower_bound: 'rep.sz',
    upper_bound: 'rep.sz',
    json: true
  })

  assert({
    given: '3 users with rep',
    should: 'have entries in rep table',
    actual: reps.rows.length,
    expected: 3
  })

  assert({
    given: '4 users with cbs',
    should: 'have entries in cbs table',
    actual: cbsAfter.rows.map(({rank})=>rank),
    expected: [0,25,50,75]
  })

  assert({
    given: 'cbs ranked in one go',
    should: 'have same order',
    actual: cbsAfter2.rows.map(({rank})=>rank),
    expected: cbsAfter.rows.map(({rank})=>rank),
  })



  assert({
    given: '3 users with rep',
    should: 'have number in sizes table',
    actual: sizes.rows[0].size,
    expected: 3
  })

  assert({
    given: '4 users with cbs',
    should: 'have number in sizes table',
    actual: sizesCBS.rows[0].size,
    expected: 4
  })


  assert({
    given: '4 users total',
    should: 'have 4 in sizes table',
    actual: userSize.rows[0].size,
    expected: 4
  })

  assert({
    given: 'removed rep',
    should: 'have entries in rep table',
    actual: repsAfter.rows.length,
    expected: 2
  })

  assert({
    given: 'removed rep from first user',
    should: 'had 100, minus 2 is 98',
    actual: repsAfterFirstUser.rows[0].rep,
    expected: 98
  })

  assert({
    given: 'removed rep',
    should: 'have number in sizes table',
    actual: sizesAfter.rows[0].size,
    expected: 2
  })

})

describe('vouching cbp earning', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ settings, accounts, onboarding })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('add users')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })

  await contracts.accounts.testsetrs(seconduser, 50, { authorization: `${accounts}@active` })

  console.log('seconduser becomes a resident and can vouch')
  await contracts.accounts.testresident(seconduser, { authorization: `${accounts}@active` })

  console.log('vouch for first user')
  await contracts.accounts.vouch(seconduser, firstuser, { authorization: `${seconduser}@active` })

  const vouchTable = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })


  console.log("firstuser becomes resident")
  await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
  const expected_cbp_res = await  get_settings("vou.cbp1.ind")

  const cbsAfterResident = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })

  // console.log("cbs "+JSON.stringify(cbsAfterResident, null, 2))

  assert({
    given: 'firstuser became resident',
    should: 'sponsor received enough cbp',
    actual: cbsAfterResident.rows[0].community_building_score,
    expected: expected_cbp_res
  })

  console.log("firstuser becomes citizen")
  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  const expected_cbp_cit = await get_settings("vou.cbp2.ind")

  const cbsAfterCitizen = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'cbs',
    json: true
  })

  // console.log("cbs "+JSON.stringify(cbsAfterCitizen, null, 2))

  assert({
    given: 'firstuser became citizen',
    should: 'sponsor received enough cbp',
    actual: cbsAfterCitizen.rows[0].community_building_score,
    expected: expected_cbp_res + expected_cbp_cit
  })

})


// TODO: Test punish
const invite = async (sponsor, totalAmount, debug = false) => {
    
    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    totalAmount = parseInt(totalAmount)

    const plantedSeeds = 5
    const transferredSeeds = totalAmount - plantedSeeds

    const transferQuantity = transferredSeeds + `.0000 SEEDS`
    const sowQuantity = plantedSeeds + '.0000 SEEDS'
    const totalQuantity = totalAmount + '.0000 SEEDS'
    
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log("     Invite Secret: "+inviteSecret)
    console.log("     Secret hash: "+inviteHash)
    
    const deposit = async () => {
        try {
            console.log(`${token}.transfer from ${sponsor} to ${onboarding} (${totalQuantity})`)
            await contracts.token.transfer(sponsor, onboarding, totalQuantity, '', { authorization: `${sponsor}@active` })        
        } catch (err) {
            console.log("deposit error: " + err)
        }
    }

    const invite = async () => {
        try {
            console.log(`${onboarding}.invite from ${sponsor}`)
            await contracts.onboarding.invite(sponsor, transferQuantity, sowQuantity, inviteHash, { authorization: `${sponsor}@active` })    
        } catch(err) {
            console.log("inv err: "+err)
        }
    }

    await deposit()

    if (debug == true) {
        const sponsorsBefore = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
        //console.log("sponsors after deposit "+JSON.stringify(sponsorsBefore.rows, null, 2))    
    }

    await invite()

    if (debug == true) {
        const sponsorsAfter = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
        //console.log("sponsors after invite "+JSON.stringify(sponsorsAfter.rows, null, 2))
    }   

    return inviteSecret

}

const accept = async (newAccount, inviteSecret, publicKey, contracts) => {
  console.log(`${onboarding}.accept invite for ${newAccount}`)
  await contracts.onboarding.accept(newAccount, inviteSecret, publicKey, { authorization: `${onboarding}@application` })        
  console.log("accept success!")
}
