const { describe } = require("riteway")
const { names, getTableRows, isLocal, initContracts, sleep, asset, getBalanceFloat, eos } = require("../scripts/helper")

const { firstuser, seconduser, thirduser, fourthuser, pool, accounts, settings, token, escrow } = names


describe('Pool transfer and payout', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ pool, accounts, settings, token })

  const users = [firstuser, seconduser, thirduser]
  const allusers = [firstuser, seconduser, thirduser, fourthuser]

  const checkBalances = async ({ expected, given, should }) => {
    const balanceTable = await getTableRows({
      code: pool,
      scope: pool,
      table: 'balances',
      json: true
    })
    const sizesTable = await getTableRows({
      code: pool,
      scope: pool,
      table: 'sizes',
      json: true
    })
    assert({
      given,
      should,
      actual: balanceTable.rows,
      expected
    })
    assert({
      given,
      should,
      actual: (sizesTable.rows.filter(r => r.id === 'total.sz')[0].size / 10000).toFixed(4),
      expected: balanceTable.rows.length > 0 ? 
        balanceTable.rows.map(r => asset(r.balance).amount).reduce((acc, curr) => acc + curr).toFixed(4) : 
        '0.0000'
    })
  }

  console.log('reset pool')
  await contracts.pool.reset({ authorization: `${pool}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('get initial balances')
  let balancesBefore = await Promise.all(users.map(user => getBalanceFloat(user)))

  console.log(`transfer to ${pool}`)
  await Promise.all(users.map((user, index) => contracts.token.transfer(user, escrow, `${10 * (index+1)}.0000 SEEDS`, '', { authorization: `${user}@active` })))
  await Promise.all(users.map((user, index) => contracts.token.transfer(escrow, pool, `${10 * (index+1)}.0000 SEEDS`, user, { authorization: `${escrow}@active` })))

  await checkBalances({
    expected: [
      { account: firstuser, balance: '10.0000 SEEDS' },
      { account: seconduser, balance: '20.0000 SEEDS' },
      { account: thirduser, balance: '30.0000 SEEDS' }
    ],
    given: 'transfered SEEDS',
    should: 'have the correct balances'
  })

  console.log('set batchsize to 2')
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })

  console.log('payout Seeds')
  await contracts.pool.payouts('10.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  await checkBalances({
    expected: [
      { account: firstuser, balance: '8.3334 SEEDS' },
      { account: seconduser, balance: '16.6667 SEEDS' },
      { account: thirduser, balance: '25.0000 SEEDS' }
    ],
    given: 'payout SEEDS',
    should: 'have the correct balances'
  })

  console.log('payout more Seeds')
  await contracts.pool.payouts('20.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(4000)

  await checkBalances({
    expected: [
      { account: firstuser, balance: '5.0001 SEEDS' },
      { account: seconduser, balance: '10.0001 SEEDS' },
      { account: thirduser, balance: '15.0001 SEEDS' }
    ],
    given: 'payout more SEEDS',
    should: 'have the correct balances'
  })

  console.log('payout all the Seeds')
  await contracts.pool.payouts('30.0003 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  await checkBalances({
    expected: [],
    given: 'payout all SEEDS',
    should: 'have the correct balances'
  })

  console.log('get balances after')
  let balancesAfter = await Promise.all(users.map(user => getBalanceFloat(user)))

  assert({
    given: 'all the payouts completed',
    should: 'have the correct Seeds balance',
    actual: balancesAfter.map((balanceAfter, index) => balanceAfter - balancesBefore[index]).reduce((acc, curr) => acc + curr),
    expected: 0
  })

  console.log('---begin pool token tests---')
  console.log('reset pool')
  await contracts.pool.reset({ authorization: `${pool}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset token')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('get initial balances')
  balancesBefore = await Promise.all(allusers.map(user => getBalanceFloat(user)))

  console.log(`transfer to ${pool}`)
  await Promise.all(users.map((user, index) => contracts.token.transfer(user, escrow, `${10 * (index+1)}.0000 SEEDS`, '', { authorization: `${user}@active` })))
  await Promise.all(users.map((user, index) => contracts.token.transfer(escrow, pool, `${10 * (index+1)}.0000 SEEDS`, user, { authorization: `${escrow}@active` })))

  assert({
    given: 'transfered SEEDS',
    should: 'have the correct HPOOL balances',
    actual: await Promise.all(users.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))),
    expected: [ ["10.0000 HPOOL"], ["20.0000 HPOOL"], ["30.0000 HPOOL"] ]
  })

  console.log('transfer HPOOL')
  await contracts.pool.transfer(seconduser, fourthuser, '5.0000 HPOOL', '', { authorization: `${seconduser}@active` })
  await contracts.pool.transfer(thirduser, fourthuser, '15.0000 HPOOL', '', { authorization: `${thirduser}@active` })

  await checkBalances({
    expected: [
      { account: firstuser, balance: '10.0000 SEEDS' },
      { account: seconduser, balance: '15.0000 SEEDS' },
      { account: thirduser, balance: '15.0000 SEEDS' },
      { account: fourthuser, balance: '20.0000 SEEDS'}
    ],
    given: 'transfered HPOOL',
    should: 'have the correct balances'
  })

  console.log('payout Seeds')
  await contracts.pool.payouts('10.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  await checkBalances({
    expected: [
      { account: firstuser, balance: '8.3334 SEEDS' },
      { account: seconduser, balance: '12.5000 SEEDS' },
      { account: thirduser, balance: '12.5000 SEEDS' },
      { account: fourthuser, balance: '16.6667 SEEDS' }
    ],
    given: 'payout SEEDS',
    should: 'have the correct balances'
  })

  console.log('transfer more HPOOL')
  await contracts.pool.transfer(firstuser, seconduser, '8.3334 HPOOL', '', { authorization: `${firstuser}@active` })

  await checkBalances({
    expected: [
      { account: seconduser, balance: '20.8334 SEEDS' },
      { account: thirduser, balance: '12.5000 SEEDS' },
      { account: fourthuser, balance: '16.6667 SEEDS' }
    ],
    given: 'transfer more HPOOL',
    should: 'have the correct balances'
  })

  console.log('payout more Seeds')
  await contracts.pool.payouts('20.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(4000)

  await checkBalances({
    expected: [
      { account: seconduser, balance: '12.5001 SEEDS' },
      { account: thirduser, balance: '7.5001 SEEDS' },
      { account: fourthuser, balance: '10.0001 SEEDS' }
    ],
    given: 'payout more SEEDS',
    should: 'have the correct balances'
  })

  console.log('payout all the Seeds')
  await contracts.pool.payouts('30.0003 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  await checkBalances({
    expected: [],
    given: 'payout all SEEDS',
    should: 'have the correct balances'
  })

  console.log('get balances after')
  balancesAfter = await Promise.all(allusers.map(user => getBalanceFloat(user)))
  balancesDiff = balancesAfter.map( (balanceAfter, index) =>
     parseFloat((balanceAfter - balancesBefore[index]).toFixed(6)) )

  assert({
    given: 'all the payouts completed',
    should: 'have the correct Seeds balance',
    actual: [ balancesDiff, balancesDiff.reduce((acc, curr) => acc + curr) ],
    expected: [ [ -8.3334, 3.3334, -15, 20 ], 0 ]
  })


})
