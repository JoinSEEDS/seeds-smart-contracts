const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, sleep, initContracts,
        httpEndpoint, getBalance, getBalanceFloat, asset } = require("../scripts/helper")
const { addActorPermission } = require("../scripts/deploy")
const { equals } = require("ramda")
const fetch = require("node-fetch");
const { escrow, accounts, token, firstuser, seconduser, thirduser, pool, fourthuser,
        fifthuser, settings, rainbows, owner } = names
const moment = require('moment')

const get_scope = async ( code ) => {
  const url = httpEndpoint + '/v1/chain/get_table_by_scope'
  const params = {
    json: "true",
    limit: 20,
    code: code
  }
  const rawResponse = await fetch(url, {
        method: 'POST',
        headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(params)
  });

  const res = await rawResponse.json();
  return res
}

const get_supply = async( code, symbol ) => {
  const resp = await getTableRows({
    code: code,
    scope: symbol,
    table: 'stat',
    json: true
  })
  const res = await resp;
  return res.rows[0].supply;
}

describe('rainbows', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

  console.log('installed at '+rainbows)
  const contracts = await Promise.all([
    eos.contract(rainbows),
    eos.contract(escrow),
    eos.contract(accounts),
    eos.contract(settings),
    eos.contract(token),
    eos.contract(pool),
  ]).then(([rainbows, escrow, accounts, settings, token, pool]) => ({
    rainbows, escrow, accounts, settings, token, pool
  }))

  const setSeedsBalance = async( account, balance) => {
    console.log(`setting ${account} to ${balance}`)
    let bal = await eos.getCurrencyBalance(token, account, 'SEEDS')
    if( bal[0] != balance ) {
      await contracts.token.transfer(account, owner, bal[0], '', { authorization: `${account}@active` } )
      if( parseFloat(balance) != 0.0 ) {
        await contracts.token.transfer(owner, account, balance, '', { authorization: `${owner}@active` } )
      }
    }
  }

  const issuer = firstuser
  const toke_escrow = seconduser
  const withdraw_to = thirduser

  const starttime = new Date()

  console.log('--Normal operations--')

  console.log('add eosio.code permissions')
  await addActorPermission(issuer, 'active', rainbows, 'eosio.code')
  await addActorPermission(toke_escrow, 'active', rainbows, 'eosio.code')

  console.log('reset')
  await contracts.rainbows.reset(true, 100, { authorization: `${rainbows}@active` })

  const accts = [ firstuser, seconduser, thirduser, fourthuser, fifthuser ]
  for( const acct of accts ) {
    await contracts.rainbows.resetacct( acct, { authorization: `${rainbows}@active` })
  }  

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(rainbows),
    expected: { rows: [], more: '' }
  })

  await setSeedsBalance(issuer, '10000000.0000 SEEDS')
  await setSeedsBalance(seconduser, '10000000.0000 SEEDS')
  await setSeedsBalance(thirduser, '5000000.0000 SEEDS')
  await setSeedsBalance(fourthuser, '10000000.0000 SEEDS')
  const issuerInitialBalance = await getBalance(issuer)

  console.log('create token')
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )

  console.log('set stake')
  await contracts.rainbows.setstake('5.00 TOKES', '2.0000 SEEDS', 'token.seeds', toke_escrow, false, 100, '',
                          { authorization: `${issuer}@active` } )

  console.log('approve token')
  await contracts.rainbows.approve('TOKES', false, { authorization: `${rainbows}@active` })

  console.log('issue tokens')
  await contracts.rainbows.issue('500.00 TOKES', '', { authorization: `${issuer}@active` })

  assert({
    given: 'create & issue token',
    should: 'see token created & issued',
    actual: await get_scope(rainbows),
    expected: {
      rows: [ {"code":"rainbo.seeds","scope":".....ou5dhbp4","table":"configs","payer":"seedsuseraaa","count":1},
              {"code":"rainbo.seeds","scope":".....ou5dhbp4","table":"displays","payer":"seedsuseraaa","count":1},
              {"code":"rainbo.seeds","scope":".....ou5dhbp4","table":"stakes","payer":"seedsuseraaa","count":2},
              {"code":"rainbo.seeds","scope":".....ou5dhbp4","table":"stat","payer":"seedsuseraaa","count":1},
              {"code":"rainbo.seeds","scope":"rainbo.seeds","table":"symbols","payer":"seedsuseraaa","count":1},
              {"code":"rainbo.seeds","scope":"seedsuseraaa","table":"accounts","payer":"seedsuseraaa","count":1} ],
      more: '' }
  })

  console.log('open accounts')
  await contracts.rainbows.open(fourthuser, 'TOKES', issuer, { authorization: `${issuer}@active` })
  await contracts.rainbows.open(withdraw_to, 'TOKES', issuer, { authorization: `${issuer}@active` })

  console.log('transfer tokens')
  await contracts.rainbows.transfer(issuer, fourthuser, '100.00 TOKES', 'test 1', { authorization: `${issuer}@active` })

  console.log('withdraw tokens')
  await contracts.rainbows.transfer(fourthuser, withdraw_to, '80.00 TOKES', 'withdraw', { authorization: `${issuer}@active` })

  assert({
    given: 'transfer token to new user',
    should: 'see tokens in users account, correct supply',
    actual: [ await eos.getCurrencyBalance(token, toke_escrow, 'SEEDS'),
              await eos.getCurrencyBalance(rainbows, fourthuser, 'TOKES'),
              await get_supply(rainbows, 'TOKES'),
            ],
    expected: [ [ '10000200.0000 SEEDS' ], [ '20.00 TOKES' ], '500.00 TOKES' ]
  })

  console.log('redeem & return')
  await contracts.rainbows.retire(fourthuser, '20.00 TOKES', 'redeemed by user', { authorization: `${fourthuser}@active` })  
  await contracts.rainbows.retire(withdraw_to, '80.00 TOKES', 'redeemed by user', { authorization: `${withdraw_to}@active` })  
  await contracts.rainbows.retire(issuer, '400.00 TOKES', 'redeemed by issuer', { authorization: `${issuer}@active` })  
  await contracts.token.transfer(fourthuser, issuer, '8.0000 SEEDS', 'restore SEEDS balance',
                       { authorization: `${fourthuser}@active` })
  await contracts.token.transfer(withdraw_to, issuer, '32.0000 SEEDS', 'restore SEEDS balance',
                       { authorization: `${withdraw_to}@active` })

  assert({
    given: 'redeem & return',
    should: 'see original seeds quantity in issuer account',
    actual: await getBalance(issuer),
    expected: issuerInitialBalance
  })

  console.log('create credit limit token')
  await contracts.rainbows.create(issuer, '1000000.00 CREDS', issuer, issuer, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await contracts.rainbows.approve('CREDS', false, { authorization: `${rainbows}@active` })
  await contracts.rainbows.issue('1000000.00 CREDS', '', { authorization: `${issuer}@active` })
  await contracts.rainbows.freeze('CREDS', true, '', { authorization: `${issuer}@active` })
  await contracts.rainbows.open(fourthuser, 'CREDS', issuer, { authorization: `${issuer}@active` })
  await contracts.rainbows.transfer(issuer, fourthuser, '100.00 CREDS', '', { authorization: `${issuer}@active` })

  console.log('reconfigure token')
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', 'CREDS', '',
                          { authorization: `${issuer}@active` } )
  console.log('make transfer against credit limit')
  await contracts.rainbows.transfer(fourthuser, issuer, '50.00 TOKES', '', { authorization: `${fourthuser}@active` })

  assert({
    given: 'transfer tokens',
    should: 'see negative currency balance',
    actual: [ await eos.getCurrencyBalance(rainbows, fourthuser, 'TOKES'),
              await get_supply(rainbows, 'TOKES') ],
    expected: [ [ '-50.00 TOKES' ], '50.00 TOKES' ]
  })

  console.log('return some TOKES')
  await contracts.rainbows.transfer(issuer, fourthuser, '20.00 TOKES', '', { authorization: `${issuer}@active` })
  assert({
    given: 'transfer tokens back',
    should: 'see expected currency balance and supply',
    actual: [ await eos.getCurrencyBalance(rainbows, fourthuser, 'TOKES'),
              await get_supply(rainbows, 'TOKES') ],
    expected: [ [ '-30.00 TOKES' ], '30.00 TOKES' ]
  })

  console.log('reset CREDS')
  let bal = await eos.getCurrencyBalance(rainbows, fourthuser, 'CREDS')
  if( bal[0] != '0.00 CREDS' ) {
    await contracts.rainbows.transfer(fourthuser, issuer, bal[0], 'withdraw CREDS', { authorization: `${issuer}@active` } )
  }


  console.log('create proportional staked token')

  await contracts.rainbows.create(issuer, '1000000.0000 PROPS', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await setSeedsBalance(fourthuser, '10000000.0000 SEEDS')


  console.log('empty PROPS escrow account')
  await setSeedsBalance(fifthuser, '0.0000 SEEDS')


  console.log('set stake')
  await contracts.rainbows.setstake('1.0000 PROPS', '2.0000 SEEDS', 'token.seeds', fifthuser, true, 100, '',
                          { authorization: `${issuer}@active` } )
  await addActorPermission(fifthuser, 'active', rainbows, 'eosio.code')

  console.log('approve token')
  await contracts.rainbows.approve('PROPS', false, { authorization: `${rainbows}@active` })

  console.log('issue tokens')
  await contracts.rainbows.issue('500.0000 PROPS', '', { authorization: `${issuer}@active` })

  assert({
    given: 'create & issue token',
    should: 'see token created & issued',
    actual: await get_scope(rainbows),
    expected: {
      rows: [ { code: 'rainbo.seeds', scope: '.....ou4cpd43', table: 'configs', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....ou4cpd43', table: 'displays', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....ou4cpd43', table: 'stat', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....ou5dhbp4', table: 'configs', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....ou5dhbp4', table: 'displays', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....ou5dhbp4', table: 'stakes', payer: 'seedsuseraaa', count: 2 },
              { code: 'rainbo.seeds', scope: '.....ou5dhbp4', table: 'stat', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....oukdxd5', table: 'configs', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....oukdxd5', table: 'displays', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: '.....oukdxd5', table: 'stakes', payer: 'seedsuseraaa', count: 2 },
              { code: 'rainbo.seeds', scope: '.....oukdxd5', table: 'stat', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: 'rainbo.seeds', table: 'symbols', payer: 'seedsuseraaa', count: 3 },
              { code: 'rainbo.seeds', scope: 'seedsuseraaa', table: 'accounts', payer: 'seedsuseraaa', count: 3 },
              { code: 'rainbo.seeds', scope: 'seedsuserccc', table: 'accounts', payer: 'seedsuseraaa', count: 1 },
              { code: 'rainbo.seeds', scope: 'seedsuserxxx', table: 'accounts', payer: 'seedsuseraaa', count: 2 }
            ],
      more: '' }
  })

  console.log('transfer tokens')
  await contracts.rainbows.transfer(issuer, fourthuser, '100.0000 PROPS', 'test nonmember', { authorization: `${issuer}@active` })

  console.log('redeem some')
  await contracts.rainbows.retire(fourthuser, '20.0000 PROPS', 'redeemed by user', { authorization: `${fourthuser}@active` })  
  

  assert({
    given: 'transfer tokens',
    should: 'see correct quantity',
    actual: [ await eos.getCurrencyBalance(rainbows, fourthuser, 'PROPS'),
              await eos.getCurrencyBalance(token, fourthuser, 'SEEDS') ],
    expected: [ [ '80.0000 PROPS' ], [ '10000040.0000 SEEDS' ] ]
  })

  console.log('increase escrow balance by 50%')
  await contracts.token.transfer(issuer, fifthuser, '480.0000 SEEDS', '+50% escrow', { authorization: `${issuer}@active` })

  console.log('redeem some more')
  await contracts.rainbows.retire(fourthuser, '20.0000 PROPS', 'redeemed by user', { authorization: `${fourthuser}@active` })  

  assert({
    given: 'proportional reedeem',
    should: 'see tokens redeemed at +50% rate',
    actual: [ await eos.getCurrencyBalance(rainbows, fourthuser, 'PROPS'),
              await eos.getCurrencyBalance(token, fourthuser, 'SEEDS') ],
    expected: [ [ '60.0000 PROPS' ], [ '10000100.0000 SEEDS' ] ]
  })

  console.log('redeem & return')
  await contracts.rainbows.retire(fourthuser, '60.0000 PROPS', 'redeemed by user', { authorization: `${fourthuser}@active` })  
  await contracts.rainbows.retire(issuer, '400.0000 PROPS', 'redeemed by issuer', { authorization: `${issuer}@active` })  
  await contracts.token.transfer(fourthuser, issuer, '280.0000 SEEDS', 'restore SEEDS balance',
                       { authorization: `${fourthuser}@active` })

  assert({
    given: 'redeem & return',
    should: 'see original seeds quantity in issuer account',
    actual: await getBalance(issuer),
    expected: issuerInitialBalance
  })

  console.log('---begin error condition checks---')

  console.log('reset')
  await contracts.rainbows.reset(true, 100, { authorization: `${rainbows}@active` })

  for( const acct of accts ) {
    await contracts.rainbows.resetacct( acct, { authorization: `${rainbows}@active` })
  }  

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(rainbows),
    expected: { rows: [], more: '' }
  })

  console.log('create token')
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )

  console.log('issue tokens without approval')
  let actionProperlyBlocked = true
  try {
    await contracts.rainbows.issue('500.00 TOKES', '', { authorization: `${issuer}@active` })
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('cannot issue until token is approved')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  assert({
    given: 'trying to issue tokens without approval',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })

  console.log('create membership token with erroneous decimals')
  await contracts.rainbows.create(issuer, '1000.00 MEMBERS', issuer, issuer, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await contracts.rainbows.approve('MEMBERS', false, { authorization: `${rainbows}@active` })
  await contracts.rainbows.freeze('MEMBERS', true, '', { authorization: `${issuer}@active` })

  console.log('update token with member token errors')
  actionProperlyBlocked = true
  try {
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), 'MEMBERS', '', '', '',
                          { authorization: `${issuer}@active` } )
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('MEMBERS token precision must be 0')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  try {
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), 'BADNAME', '', '', '',
                          { authorization: `${issuer}@active` } )
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('BADNAME token does not exist')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  assert({
    given: 'trying to create token with bad membership',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })
 
  console.log('create good membership token')
  await contracts.rainbows.create(issuer, '1000 MEMBERS', issuer, issuer, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await contracts.rainbows.approve('MEMBERS', false, { authorization: `${rainbows}@active` })
  await contracts.rainbows.freeze('MEMBERS', true, '', { authorization: `${issuer}@active` })
  await contracts.rainbows.issue('100 MEMBERS', '', { authorization: `${issuer}@active` })
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), 'MEMBERS', '', '', '',
                          { authorization: `${issuer}@active` } )
  await contracts.rainbows.approve('TOKES', false, { authorization: `${rainbows}@active` })
  await contracts.rainbows.issue('1000.00 TOKES', '', { authorization: `${issuer}@active` })

  console.log('send tokens against membership rules')
  actionProperlyBlocked = true
  try {
    await contracts.rainbows.transfer(issuer, fourthuser, '100.00 TOKES', '', { authorization: `${issuer}@active` })
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('to account must have membership')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  try {
    await contracts.rainbows.transfer(issuer, fourthuser, '1 MEMBERS', '', { authorization: `${issuer}@active` })
    await contracts.rainbows.transfer(issuer, fifthuser, '1 MEMBERS', '', { authorization: `${issuer}@active` })
    await contracts.rainbows.transfer(issuer, fourthuser, '100.00 TOKES', '', { authorization: `${issuer}@active` })
    await contracts.rainbows.transfer(fourthuser, fifthuser, '100.00 TOKES', '', { authorization: `${fourthuser}@active` })
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('cannot transfer visitor to visitor')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  assert({
    given: 'trying to send tokens against membership rules',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })

  console.log('send tokens between visitor and member')
    await contracts.rainbows.transfer(issuer, fifthuser, '1 MEMBERS', '', { authorization: `${issuer}@active` })
    await contracts.rainbows.transfer(fourthuser, fifthuser, '100.00 TOKES', '', { authorization: `${fourthuser}@active` })
    await contracts.rainbows.transfer(fifthuser, fourthuser, '40.00 TOKES', '', { authorization: `${fifthuser}@active` })
  assert({
    given: 'valid transfers',
    should: 'see correct token balance in recipient account',
    actual: await eos.getCurrencyBalance(rainbows, fifthuser, 'TOKES'),
    expected: [ '60.00 TOKES' ]
  })

  console.log('create fractional staked token')

  await contracts.rainbows.create(issuer, '1000000.0000 FRACS', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await setSeedsBalance(fourthuser, '10000000.0000 SEEDS')


  console.log('empty FRACS escrow account')
  await setSeedsBalance(fifthuser, '0.0000 SEEDS')


  console.log('set stake')
  await contracts.rainbows.setstake('1.0000 FRACS', '2.0000 SEEDS', 'token.seeds', fifthuser, false, 30, '',
                          { authorization: `${issuer}@active` } )
  await addActorPermission(fifthuser, 'active', rainbows, 'eosio.code')

  console.log('approve token')
  await contracts.rainbows.approve('FRACS', false, { authorization: `${rainbows}@active` })

  console.log('issue tokens')
  await contracts.rainbows.issue('500.0000 FRACS', '', { authorization: `${issuer}@active` })

  console.log('transfer tokens')
  await contracts.rainbows.transfer(issuer, fourthuser, '100.0000 FRACS', '', { authorization: `${issuer}@active` })
  await contracts.token.transfer(fifthuser, issuer, '500.0000 SEEDS', '', { authorization: `${fifthuser}@active` })

  console.log('redeem some')
  await contracts.rainbows.retire(fourthuser, '20.0000 FRACS', 'redeemed by user', { authorization: `${fourthuser}@active` })  
  

  assert({
    given: 'create & redeem with fractional reserve',
    should: 'see correct quantity',
    actual: [ await eos.getCurrencyBalance(rainbows, fourthuser, 'FRACS'),
              await eos.getCurrencyBalance(token, fourthuser, 'SEEDS'),
              await eos.getCurrencyBalance(token, fifthuser, 'SEEDS') ],
    expected: [ [ '80.0000 FRACS' ], [ '10000040.0000 SEEDS' ], [ '460.0000 SEEDS' ] ]
  })

  console.log('redeem with insufficient reserve')
  actionProperlyBlocked = true
  try {
    await contracts.token.transfer(fifthuser, issuer, '145.0000 SEEDS', '', { authorization: `${fifthuser}@active` })
    await contracts.rainbows.retire(fourthuser, '20.0000 FRACS', 'redeemed by user', { authorization: `${fourthuser}@active` })  
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('can\'t unstake, escrow underfunded in SEEDS')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  assert({
    given: 'trying to redeem with insufficient reserve',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })

  console.log('---begin dSeeds stake tests---')

  const dseed_escrow = fifthuser

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

  const users = [firstuser, seconduser, thirduser]
  const allusers = [firstuser, seconduser, thirduser, fourthuser, fifthuser]

  console.log('reset')
  await contracts.rainbows.reset(true, 100, { authorization: `${rainbows}@active` })

  for( const acct of accts ) {
    await contracts.rainbows.resetacct( acct, { authorization: `${rainbows}@active` })
  }  

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(rainbows),
    expected: { rows: [], more: '' }
  })

  console.log('create dSeed-staked token ARCOS')

  await contracts.rainbows.create(issuer, '1000000.0000 ARCOS', issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '', '', '',
                          { authorization: `${issuer}@active` } )
  await setSeedsBalance(issuer, '10000000.0000 SEEDS')
  await setSeedsBalance(seconduser, '10000000.0000 SEEDS')
  await setSeedsBalance(thirduser, '5000000.0000 SEEDS')
  await setSeedsBalance(fourthuser, '10000000.0000 SEEDS')


  console.log('empty dSeeds escrow account')
  await setSeedsBalance(dseed_escrow, '0.0000 SEEDS')
  bal = await eos.getCurrencyBalance(pool, dseed_escrow, 'HPOOL')
  //if( bal[0] != '0.00000 HPOOL' ) {
  //  await contracts.pool.transfer(dseed_escrow, issuer, bal[0], 'empty dseed_escrow', { authorization: `${issuer}@active` } )
  //}


  const setupPool = async () => {
    console.log('reset pool')
    await contracts.pool.reset({ authorization: `${pool}@active` })

    console.log('reset accounts')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('reset settings')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('reset token')
    await contracts.token.resetweekly({ authorization: `${token}@active` })
  }
  await setupPool()

  console.log('get initial balances')
  balancesBefore = await Promise.all(allusers.map(user => getBalanceFloat(user)))

  console.log(`transfer to ${pool}`)
  await Promise.all(users.map((user, index) => contracts.token.transfer(user, escrow, `1000.0000 SEEDS`, '', { authorization: `${user}@active` })))
  await Promise.all(users.map((user, index) => contracts.token.transfer(escrow, pool, `1000.0000 SEEDS`, user, { authorization: `${escrow}@active` })))
  await contracts.pool.transfer(issuer, seconduser, '1000.0000 HPOOL', '', { authorization: `${issuer}@active` })

  assert({
    given: 'transfered SEEDS',
    should: 'have the correct HPOOL balances',
    actual: await Promise.all(users.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))),
    expected: [ [], ["2000.0000 HPOOL"], ["1000.0000 HPOOL"] ]
  })

  console.log('transfer HPOOL to issuer')
  await contracts.pool.transfer(thirduser, issuer, '500.0000 HPOOL', '', { authorization: `${thirduser}@active` })

  console.log('set placeholder Seeds stake')
  await contracts.rainbows.setstake('1.0000 ARCOS', '0.0000 SEEDS', 'token.seeds', dseed_escrow, true, 100, '',
                          { authorization: `${issuer}@active` } )
  await addActorPermission(dseed_escrow, 'active', rainbows, 'eosio.code')

  console.log('set dSeeds stake')
  await contracts.rainbows.setstake('1.0000 ARCOS', '1.0000 HPOOL', 'pool.seeds', dseed_escrow, true, 100, '',
                          { authorization: `${issuer}@active` } )

  console.log('approve token')
  await contracts.rainbows.approve('ARCOS', false, { authorization: `${rainbows}@active` })


  console.log('issue ARCOS tokens')
  await contracts.rainbows.issue('500.0000 ARCOS', '', { authorization: `${issuer}@active` })

  assert({
    given: 'issued ARCOS',
    should: 'have the correct SEEDS & HPOOL balances',
    actual: [ await Promise.all(allusers.map(user => eos.getCurrencyBalance(token, user, 'SEEDS'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))) ],
    expected: [ [ [ '9999000.0000 SEEDS' ], [ '9999000.0000 SEEDS' ], [ '4999000.0000 SEEDS' ], [ '10000000.0000 SEEDS' ], [ '0.0000 SEEDS' ] ],
                [ [], [ '2000.0000 HPOOL' ], [ '500.0000 HPOOL' ], [], [ '500.0000 HPOOL' ] ] ]
  })

  console.log('transfer ARCOS to user')
  await contracts.rainbows.transfer(issuer, fourthuser, '300.0000 ARCOS', 'test nonmember', { authorization: `${issuer}@active` })
  assert({
    given: 'transferred to user',
    should: 'have the correct ARCOS balances',
    actual: await Promise.all(allusers.map(user => eos.getCurrencyBalance(rainbows, user, 'ARCOS'))),
    expected: [ [ '200.0000 ARCOS' ], [], [], [ '300.0000 ARCOS' ], [] ]
  })

  console.log('harvest payout Seeds from pool')
  await contracts.pool.payouts('60.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  await checkBalances({
    expected: [
      { account: seconduser, balance: '1960.0000 SEEDS' },
      { account: thirduser, balance: '490.0000 SEEDS' },
      { account: dseed_escrow, balance: '490.0000 SEEDS' }
    ],
    given: 'harvest payout',
    should: 'have the correct pool balances'
  })

  console.log('user redeem some')
  await contracts.rainbows.retire(fourthuser, '150.0000 ARCOS', 'redeemed by user', { authorization: `${fourthuser}@active` })  
  
  assert({
    given: 'user redeem',
    should: 'have the correct SEEDS, HPOOL, & ARCOS balances',
    actual: [ await Promise.all(allusers.map(user => eos.getCurrencyBalance(token, user, 'SEEDS'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(rainbows, user, 'ARCOS'))) ],
    expected: [ [ [ '9999000.0000 SEEDS' ], [ '9999040.0000 SEEDS' ], [ '4999010.0000 SEEDS' ], [ '10000003.0000 SEEDS' ], [ '7.0000 SEEDS' ] ],
                [ [], [ '1960.0000 HPOOL' ], [ '490.0000 HPOOL' ], [ '147.0000 HPOOL' ], [ '343.0000 HPOOL' ] ],
                [ [ '200.0000 ARCOS' ], [], [], [ '150.0000 ARCOS' ], [] ] ]
  })


  console.log('payout all the Seeds in pool')
  await contracts.pool.payouts('2940.0000 SEEDS', { authorization: `${pool}@active` })
  await sleep(2000)

  assert({
    given: 'payout all Seeds in pool',
    should: 'have the correct SEEDS, HPOOL, & ARCOS balances',
    actual: [ await Promise.all(allusers.map(user => eos.getCurrencyBalance(token, user, 'SEEDS'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(rainbows, user, 'ARCOS'))) ],
    expected: [ [ [ '9999000.0000 SEEDS' ], [ '10001000.0000 SEEDS' ], [ '4999500.0000 SEEDS' ], [ '10000150.0000 SEEDS' ], [ '350.0000 SEEDS' ] ],
                [ [], [], [], [], [] ],
                [ [ '200.0000 ARCOS' ], [], [], [ '150.0000 ARCOS' ], [] ] ]
  })

  console.log('redeem all')
  await contracts.rainbows.retire(issuer, '200.0000 ARCOS', 'redeemed by issuer', { authorization: `${issuer}@active` })  
  await contracts.rainbows.retire(fourthuser, '150.0000 ARCOS', 'redeemed by user', { authorization: `${fourthuser}@active` })  

  assert({
    given: 'all the payouts completed and redeemed',
    should: 'have the correct SEEDS, HPOOL, & ARCOS balances',
    actual: [ await Promise.all(allusers.map(user => eos.getCurrencyBalance(token, user, 'SEEDS'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(pool, user, 'HPOOL'))),
              await Promise.all(allusers.map(user => eos.getCurrencyBalance(rainbows, user, 'ARCOS'))) ],
    expected: [ [ [ '9999200.0000 SEEDS' ], [ '10001000.0000 SEEDS' ], [ '4999500.0000 SEEDS' ], [ '10000300.0000 SEEDS' ], [ '0.0000 SEEDS' ] ],
                [ [], [], [], [], [] ],
                [ [ '0.0000 ARCOS' ], [], [], [ '0.0000 ARCOS' ], [] ] ]
  })


    
})


