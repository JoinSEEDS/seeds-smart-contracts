const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, initContracts, getBalance, sleep } = require("../scripts/helper")
const eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

const { firstuser, seconduser, thirduser, fourthuser, settings, gratitude, accounts, token, bank } = names

describe('gratitude general', async assert => {

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
  
  const getRemainingGratitude = async account => {
    const balanceTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'balances',
      json: true
    })
    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].remaining)
  }
  
  const checkRemainingGratitude = async (account, expected) => {
    const remaining = await getRemainingGratitude(account)
    assert({
      given: `${account} given gratitude`,
      should: 'have the correct remaining balance',
      actual: remaining,
      expected: expected
    })
  }
  
  const getReceivedGratitude = async account => {
    const balanceTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'balances',
      json: true
    })
    const balance = balanceTable.rows.filter(r => r.account == account)
    return parseInt(balance[0].received)
  }

  const getGratitudeStats = async () => {
    const statsTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'stats2',
      json: true
    })
    if (statsTable.rows) {
      return statsTable.rows[statsTable.rows.length-1];
    }
    return [];
  }

  const checkRoundPot = async (expected) => {
    const grstats = await getGratitudeStats()
    assert({
      given: `received deposit`,
      should: 'have the correct pot balance',
      actual: grstats.round_pot,
      expected: expected
    })
  }

  const checkRoundTransfers = async (expected) => {
    const grstats = await getGratitudeStats()
    assert({
      given: `given gratitude directly`,
      should: 'have the correct num transfers',
      actual: grstats.num_transfers,
      expected: expected
    })
  }

  const checkRoundAcks = async (expected) => {
    const grstats = await getGratitudeStats()
    assert({
      given: `performed ack`,
      should: 'have the correct num acks',
      actual: grstats.num_acks,
      expected: expected
    })
  }

  const checkRoundVolume = async (expected) => {
    const grstats = await getGratitudeStats()
    assert({
      given: `performed transfer`,
      should: 'have the correct volume',
      actual: grstats.volume,
      expected: expected
    })
  }

  const checkReceivedGratitude = async (account, expected) => {
    const received = await getReceivedGratitude(account)
    assert({
      given: `${account} received gratitude`,
      should: 'have the correct received balance',
      actual: received,
      expected: expected
    })
  }

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ gratitude, accounts, settings, token })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('gratitude reset')
  await contracts.gratitude.reset({ authorization: `${gratitude}@active` })

  console.log('reset contract balance')
  const stale_balance = await getBalance(gratitude)
  await contracts.token.transfer(gratitude, firstuser, `${stale_balance}.0000 SEEDS`, 'test', { authorization: `${gratitude}@active` })

  await contracts.settings.configure("batchsize", 1, { authorization: `${settings}@active` })

  const initialGratitude = await get_settings("gratz1.gen") / 10000; // in GRATZ

  console.log('add SEEDS users')
  const users = [firstuser, seconduser, thirduser, fourthuser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
  }

  const amount = 1000
  const transferAmount = 10

  const contractBalanceBefore = await getBalance(gratitude) || 0
  console.log('current contract bal: '+contractBalanceBefore)
  console.log('refill gratitude contract')
  await contracts.token.transfer(firstuser, gratitude, `${amount}.0000 SEEDS`, 'test', { authorization: `${firstuser}@active` })
  checkRoundPot(`${contractBalanceBefore+amount}.0000 SEEDS`)

  console.log('give gratitude')
  await contracts.gratitude.give(firstuser, seconduser, `${transferAmount}.0000 GRATZ`, '', { authorization: `${firstuser}@active` })

  checkRoundTransfers(1)
  checkRoundVolume(`${transferAmount}.0000 GRATZ`)

  checkRemainingGratitude(firstuser, initialGratitude-transferAmount)
  checkReceivedGratitude(seconduser, transferAmount)

  console.log('test acknowledge')
  await contracts.gratitude.acknowledge(seconduser, firstuser, 'Thanks!', { authorization: `${seconduser}@active` })

  // TODO check if round status updated (volume, transfers, acks)
  checkRoundAcks(1)

  console.log('test acks non recursive')
  await contracts.gratitude.testacks({ authorization: `${gratitude}@active` })

  const gratzAcks = await get_settings("gratz.acks")
  const transferPerAckAmount = initialGratitude / gratzAcks
  checkRemainingGratitude(seconduser, initialGratitude-transferPerAckAmount)
  checkReceivedGratitude(firstuser, transferPerAckAmount)

  const firstBalanceBefore = await getBalance(firstuser)  
  const secondBalanceBefore = await getBalance(seconduser)

  console.log('new round')
  await contracts.gratitude.newround({ authorization: `${gratitude}@active` })
  await sleep(1000) // wait for parallel processing

  const contractBalanceAfter = await getBalance(gratitude)
  const firstBalanceAfter = await getBalance(firstuser)
  const secondBalanceAfter = await getBalance(seconduser)

  checkRoundPot(`${contractBalanceAfter}.0000 SEEDS`)

  //console.log('stats: '+ JSON.stringify(await getGratitudeStats()))

  assert({
    given: 'gratitude round finished',
    should: 'reset contract balance',
    actual: contractBalanceAfter,
    expected: contractBalanceBefore
  })

  assert({
    given: 'gratitude round finished',
    should: 'second user received seeds because of direct direct gratz received',
    actual: secondBalanceAfter,
    expected: secondBalanceBefore + amount/2
  })

  assert({
    given: 'gratitude round finished',
    should: 'first user received seeds because of acknowledge',
    actual: firstBalanceAfter,
    expected: firstBalanceBefore + amount/2
  })

})


describe('gratitude many acks', async assert => {
 
  const getGratitudeStats = async () => {
    const statsTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'stats2',
      json: true
    })
    if (statsTable.rows) {
      return statsTable.rows[statsTable.rows.length-1];
    }
    return [];
  }

  const checkRoundAcks = async (expected) => {
    const grstats = await getGratitudeStats()
    assert({
      given: `total performed ack`,
      should: 'have the correct num acks',
      actual: grstats.num_acks,
      expected: expected
    })
  }

  const getAcks = async (account) => {
    const acksTable = await getTableRows({
      code: 'gratz.seeds',
      scope: 'gratz.seeds',
      table: 'acks',
      json: true
    })
    if (acksTable.rows) {
      var acks =  acksTable.rows.reduce(function(map, obj) {
        map[obj.donor] = obj.receivers;
        return map;
      }, {})
      return acks[account]
    }
    return 0;
  }

  const checkUserAcks = async (user, expected) => {
    const acks = await getAcks(user)
    assert({
      given: `${user} performed ack`,
      should: 'have the correct num acks',
      actual: acks.length,
      expected: expected
    })
  }

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ gratitude, accounts, settings, token })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('gratitude reset')
  await contracts.gratitude.reset({ authorization: `${gratitude}@active` })

  await contracts.settings.configure("batchsize", 1, { authorization: `${settings}@active` })

  console.log('add SEEDS users')
  const users = [firstuser, seconduser, thirduser, fourthuser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
  }

  const amount = 1000

  const contractBalanceBefore = await getBalance(gratitude) || 0

  console.log('refill gratitude contract')
  await contracts.token.transfer(firstuser, gratitude, `${amount}.0000 SEEDS`, 'test', { authorization: `${firstuser}@active` })

  const balancesBefore = [await getBalance(firstuser), await getBalance(seconduser), await getBalance(thirduser), await getBalance(fourthuser)]

  console.log('test ack 1')
  await contracts.gratitude.acknowledge(firstuser, seconduser, 'Thanks!', { authorization: `${firstuser}@active` })

  console.log('test ack 2')
  await contracts.gratitude.acknowledge(firstuser, fourthuser, 'Thanks!', { authorization: `${firstuser}@active` })

  console.log('test ack 3')
  await contracts.gratitude.acknowledge(firstuser, thirduser, 'Thanks!', { authorization: `${firstuser}@active` })

  console.log('test ack 4')
  await contracts.gratitude.acknowledge(seconduser, thirduser, 'Thanks!', { authorization: `${seconduser}@active` })

  console.log('test ack 5')
  await contracts.gratitude.acknowledge(seconduser, fourthuser, 'Thanks!', { authorization: `${seconduser}@active` })

  console.log('test ack 6')
  await contracts.gratitude.acknowledge(seconduser, firstuser, 'Thanks!', { authorization: `${seconduser}@active` })

  console.log('test ack 7')
  await contracts.gratitude.acknowledge(thirduser, firstuser, 'Thanks!', { authorization: `${thirduser}@active` })

  // TODO check if round status updated (volume, transfers, acks)
  checkRoundAcks(7)

  checkUserAcks(firstuser, 3)

  //console.log("acks:"+ JSON.stringify(await getAcks(firstuser)))

  console.log('new round')
  await contracts.gratitude.newround({ authorization: `${gratitude}@active` })
  await sleep(5000) // wait for parallel processing

  const contractBalanceAfter = await getBalance(gratitude)
  const balancesAfter = [await getBalance(firstuser), await getBalance(seconduser), await getBalance(thirduser), await getBalance(fourthuser)]

  const statsTable = await getTableRows({
    code: gratitude,
    scope: gratitude,
    table: 'stats2',
    json: true
  })

  assert({
    given: 'gratitude round finished',
    should: 'first round volume is correct',
    actual: parseFloat(statsTable["rows"][0]["volume"]),
    expected: 70.0
  })

  assert({
    given: 'gratitude round finished',
    should: 'second user received seeds because of acknowledge',
    actual: balancesAfter[1],
    expected: balancesBefore[1] + 143
  })

  assert({
    given: 'gratitude round finished',
    should: 'reset contract balance',
    actual: contractBalanceAfter,
    expected: contractBalanceBefore
  })
})

describe('testing pot keep', async assert => {
 
  const getGratitudeStats = async () => {
    const statsTable = await getTableRows({
      code: gratitude,
      scope: gratitude,
      table: 'stats2',
      json: true
    })
    if (statsTable.rows) {
      return statsTable.rows[statsTable.rows.length-1];
    }
    return [];
  }


  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ gratitude, accounts, settings, token })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  
  console.log('token reset weekly')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('gratitude reset')
  await contracts.gratitude.reset({ authorization: `${gratitude}@active` })

  console.log('add SEEDS users')
  const users = [firstuser, seconduser]
  for (const user of users) {
    await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
  }

  const amount = 1000

  const pot_keep = 10

  // set potkp to 10%
  await contracts.settings.configure("gratz.potkp", pot_keep, { authorization: `${settings}@active` })

  console.log('refill gratitude contract')
  await contracts.token.transfer(firstuser, gratitude, `${amount}.0000 SEEDS`, 'test', { authorization: `${firstuser}@active` })

  const balancesBefore = [await getBalance(firstuser), await getBalance(seconduser)]

  console.log('do one ack')
  await contracts.gratitude.acknowledge(firstuser, seconduser, 'Thanks!', { authorization: `${firstuser}@active` })

  console.log('new round')
  await contracts.gratitude.newround({ authorization: `${gratitude}@active` })
  await sleep(4000) 

  const contractBalanceAfter = await getBalance(gratitude)
  const balancesAfter = [await getBalance(firstuser), await getBalance(seconduser)]

  const statsTable = await getTableRows({
    code: gratitude,
    scope: gratitude,
    table: 'stats2',
    json: true
  })

  assert({
    given: 'gratitude round finished',
    should: 'second round pot has correct remaining quantity',
    actual: parseFloat(statsTable["rows"][1]["round_pot"]),
    expected: amount * pot_keep / 100
  })

  assert({
    given: 'gratitude round finished',
    should: 'second user received seeds because of acknowledge',
    actual: balancesAfter[1],
    expected: balancesBefore[1] + amount - (amount * pot_keep / 100)
  })

  assert({
    given: 'gratitude round finished',
    should: 'have enough on pot based on settings',
    actual: contractBalanceAfter,
    expected: amount * pot_keep / 100
  })
})
