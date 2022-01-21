const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, sleep, initContracts, httpEndpoint, getBalance } = require("../scripts/helper")
const { addActorPermission } = require("../scripts/deploy")
const { equals } = require("ramda")
const fetch = require("node-fetch");
const { escrow, accounts, token, firstuser, seconduser, thirduser, pool, fourthuser, settings, rainbows } = names
const moment = require('moment')

const get_scope = async ( code ) => {
  const url = httpEndpoint + '/v1/chain/get_table_by_scope'
  const params = {
    json: "true",
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
    eos.contract(token),
  ]).then(([rainbows, escrow, accounts, token]) => ({
    rainbows, escrow, accounts, token
  }))


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

  const accts = [ firstuser, seconduser, thirduser, fourthuser ]
  for( const acct of accts ) {
    await contracts.rainbows.resetacct( acct, { authorization: `${rainbows}@active` })
  }  

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(rainbows),
    expected: { rows: [], more: '' }
  })

  const issuerInitialBalance = await getBalance(issuer)

  console.log('create token')
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '',
                          { authorization: `${issuer}@active` } )

  console.log('set stake')
  await contracts.rainbows.setstake('5.00 TOKES', '2.0000 SEEDS', 'token.seeds', toke_escrow, false, '',
                          { authorization: `${issuer}@active` } )

  console.log('approve token')
  await contracts.rainbows.approve('TOKES', false, { authorization: `${rainbows}@active` })

  console.log('issue tokens')
  await contracts.rainbows.issue('500.00 TOKES', '', { authorization: `${issuer}@active` })

  assert({
    given: 'create & issue token',
    should: 'token created & issued',
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
    should: 'see tokens in users account',
    actual: [ await eos.getCurrencyBalance(token, toke_escrow, 'SEEDS'),
              await eos.getCurrencyBalance(rainbows, fourthuser, 'TOKES'),
            ],
    expected: [ [ '10000200.0000 SEEDS' ], [ '20.00 TOKES' ] ]
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
  await contracts.rainbows.create(issuer, '1000000.00 CREDS', issuer, issuer, issuer, issuer,
                         starttime.toISOString(), starttime.toISOString(), '', '',
                          { authorization: `${issuer}@active` } )
  await contracts.rainbows.approve('CREDS', false, { authorization: `${rainbows}@active` })
  await contracts.rainbows.issue('1000000.00 CREDS', '', { authorization: `${issuer}@active` })
  await contracts.rainbows.freeze('CREDS', true, '', { authorization: `${issuer}@active` })
  await contracts.rainbows.open(fourthuser, 'CREDS', issuer, { authorization: `${issuer}@active` })
  await contracts.rainbows.transfer(issuer, fourthuser, '100.00 CREDS', '', { authorization: `${issuer}@active` })

  console.log('reconfigure token')
  await contracts.rainbows.create(issuer, '1000000.00 TOKES', issuer, issuer, withdraw_to, issuer,
                         starttime.toISOString(), starttime.toISOString(), 'CREDS', '',
                          { authorization: `${issuer}@active` } )
  console.log('make transfer against credit limit')
  await contracts.rainbows.transfer(fourthuser, issuer, '50.00 TOKES', '', { authorization: `${fourthuser}@active` })

  
})

/*
describe('vest and escrow', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }
    
    const contracts = await Promise.all([
        eos.contract(escrow),
        eos.contract(accounts),
        eos.contract(token),
    ]).then(([escrow, accounts, token]) => ({
        escrow, accounts, token
    }))

    console.log('escrow reset')
    await contracts.escrow.reset({ authorization: `${escrow}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` }) 

    console.log('create balance')
    await contracts.token.transfer(firstuser, escrow, "150.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, escrow, "50.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const initialBalances = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'sponsors',
        json: true
    })

    const vesting_date_passed = moment().utc().subtract(100, 's').toString()
    const vesting_date_future = moment().utc().add(1000, 's').toString()
    
    console.log('create escrow')
    await contracts.escrow.lock("time", firstuser, seconduser, '20.0000 SEEDS', ".", "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })
    await contracts.escrow.lock("time", seconduser, firstuser, '50.0000 SEEDS', ".", "seconduser", vesting_date_future, "notes", { authorization: `${seconduser}@active` })

    const initialEscrows = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'locks',
        json: true
    })

    try {
        console.log('creating an escrow without enough balance')
        await contracts.escrow.lock("time", firstuser, seconduser, '200.0000 SEEDS', ".", "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })
    }
    catch(err) {
        console.log(err.json.error.details[0].message.replace('assertion failure with message: ', ''))
    }

    let claimBeforeClaimShouldBeFalse = false
    try {
        console.log('try to claim before vesting date')
        await contracts.escrow.claim(firstuser, { authorization: `${firstuser}@active` })
        console.log('Error: Claim before vesting date worked')
        claimBeforeClaimShouldBeFalse = true
    }
    catch(err) {
        console.log(err.json.error.details[0].message.replace('assertion failure with message: ', ''))
    }

    const secondUserBalanceBefore = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    console.log('claim escrow')
    await contracts.escrow.claim(seconduser, { authorization: `${seconduser}@active` })

    const escrows = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'locks',
        json: true
    })

    const secondUserBalanceAfter = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    const middleBalances = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'sponsors',
        json: true
    })

    // TODO put this back in!
    // console.log('cancel escrow')
    // await contracts.escrow.cancelescrow(seconduser, firstuser, { authorization: `${seconduser}@active` })

    const escrowsAfter = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'locks',
        json: true
    })

    const lastBalances = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'sponsors',
        json: true
    })

    await contracts.token.transfer(seconduser, escrow, "30.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const secondUserBalanceBeforewithdraw = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    console.log('withdraw '+ JSON.stringify(secondUserBalanceBeforewithdraw, null, 2))

    await contracts.escrow.withdraw(seconduser, '30.0000 SEEDS', { authorization: `${seconduser}@active` })

    const withdrawBalances = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'sponsors',
        json: true
    })

    const secondUserBalanceAfterwithdraw = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })
    console.log('after withdraw '+ JSON.stringify(secondUserBalanceAfterwithdraw, null, 2))


    console.log('claim several escrows')
    await contracts.escrow.lock("time", firstuser, seconduser, '1.0000 SEEDS', ".", "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, seconduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_future, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, thirduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, thirduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_future, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, seconduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, seconduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_future, "notes", { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.escrow.lock("time", firstuser, seconduser, '1.0000 SEEDS', ".",  "firstuser", vesting_date_passed, "notes", { authorization: `${firstuser}@active` })

    const severalEscrowsBeforeClaim = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'locks',
        json: true
    })
    
    const expectedEscrowLocks = 7

    await contracts.escrow.claim(seconduser, { authorization: `${seconduser}@active` })

    const expectedAdditionalSeeds = 3

    const expectedRemainingLocks = 4

    const secondUserBalanceAfterThreeEscrows = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })
    
    const severalEscrowsAfter = await getTableRows({
        code: escrow,
        scope: escrow,
        table: 'locks',
        json: true
    })

    assert({
        given: 'attempt to claim before vesting date',
        should: 'fail',
        actual: claimBeforeClaimShouldBeFalse,
        expected: false
    })

    assert({
        given: 'the first and second user have transfered tokens to the vest and escrow contract',
        should: 'create the sponsors entries in the sponsors table',
        actual: initialBalances.rows.map(row => row),
        expected: [
            {
                "sponsor": "seedsuseraaa",
                "locked_balance": "0.0000 SEEDS",
                "liquid_balance": "150.0000 SEEDS"
              },
              {
                "sponsor": "seedsuserbbb",
                "locked_balance": "0.0000 SEEDS",
                "liquid_balance": "50.0000 SEEDS"
              }]
    })

    let iinitialEscrowRows = initialEscrows.rows.map( row => {
        delete row.vesting_date
        delete row.created_date
        delete row.updated_date
        return row
    })

    assert({
        given: 'the initial escrows',
        should: 'create the correspondent entries in the escrows table',
        actual: iinitialEscrowRows,
        expected: [
            {
                "id": 0,
                "lock_type": "time",
                "sponsor": "seedsuseraaa",
                "beneficiary": "seedsuserbbb",
                "quantity": "20.0000 SEEDS",
                "trigger_event": "",
                "trigger_source": "firstuser",
                "notes": "notes",
              },
              {
                "id": 1,
                "lock_type": "time",
                "sponsor": "seedsuserbbb",
                "beneficiary": "seedsuseraaa",
                "quantity": "50.0000 SEEDS",
                "trigger_event": "",
                "trigger_source": "seconduser",
                "notes": "notes",
              }
        ]
    })

    assert({
        given: 'the seconduser has claimed its escrow',
        should: 'give the funds to the user',
        actual: parseFloat(secondUserBalanceAfter.rows[0].balance),
        expected: parseFloat(secondUserBalanceBefore.rows[0].balance) + 20
        
    })

    assert({
        given: 'the second user has claimed its escrow',
        should: 'erase its entry in the escrows table',
        actual: escrows.rows.length,
        expected: 1
    })

    assert({
        given: 'before the escrow was canceled',
        should: 'get the sponsors balances correct',
        actual: middleBalances.rows.map(row => row),
        expected: [
            { sponsor: 'seedsuseraaa', locked_balance: "0.0000 SEEDS", liquid_balance: '130.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', locked_balance: "50.0000 SEEDS", liquid_balance: '0.0000 SEEDS' }
        ]
    })

    // assert({
    //     given: 'after the escrow was canceled',
    //     should: 'update the sponsors balances correctly',
    //     actual: lastBalances.rows.map(row => row),
    //     expected: [
    //         { sponsor: 'seedsuseraaa', balance: '130.0000 SEEDS' },
    //         { sponsor: 'seedsuserbbb', balance: '50.0000 SEEDS' }
    //     ]
    // })

    // assert({
    //     given: 'after the escrow was canceled',
    //     should: 'erase the escrow table entry',
    //     actual: escrowsAfter.rows.map(row => row),
    //     expected: []
    // })

    assert({
        given: 'the withdraw action called',
        should: 'withdraw the tokens to the user',
        actual: parseFloat(secondUserBalanceAfterwithdraw.rows[0].balance),
        expected: parseFloat(secondUserBalanceBeforewithdraw.rows[0].balance) + 30
    })

    assert({
        given: 'the withdraw action called',
        should: 'update the sponsors table correctly',
        actual: withdrawBalances.rows.map(row => row),
        expected: [      
            { sponsor: 'seedsuseraaa', locked_balance: "0.0000 SEEDS", liquid_balance: '130.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', locked_balance: "50.0000 SEEDS", liquid_balance: '0.0000 SEEDS' }
        ]
    })

    assert({
        given: 'several escrows created',
        should: 'have the correct number of escrows',
        actual: severalEscrowsBeforeClaim.rows.length,
        expected: 8
    })

    assert({
        given: 'claimed available locks',
        should: 'gained 3 seeds',
        actual: parseFloat(secondUserBalanceAfterThreeEscrows.rows[0].balance).toFixed(4),
        expected: (parseFloat(secondUserBalanceAfterwithdraw.rows[0].balance) + 3).toFixed(4)
    })

    assert({
        given: 'several escrows created',
        should: 'claim all available escrows',
        actual: severalEscrowsAfter.rows.length,
        expected: 5
    })
})

describe('go live', async assert => {

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

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('pool reset')
    await contracts.pool.reset({ authorization: `${pool}@active` })

    console.log('reset token')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    const checkLocks = async ({ expectedLocks, given, should }) => {
        const lockTable = await eos.getTableRows({
          code: escrow,
          scope: escrow,
          table: 'locks',
          json: true,
        })
        const locks = lockTable.rows.map(r => {
          return {
            lock_type: r.lock_type,
            sponsor: r.sponsor,
            beneficiary: r.beneficiary,
            quantity: r.quantity,
            trigger_event: r.trigger_event,
            trigger_source: r.trigger_source
          }
        }).sort((a, b) => { return a.beneficiary < b.beneficiary ? -1 : 1 })
        assert({
          given,
          should,
          actual: locks,
          expected: expectedLocks
        })
    }

    const checkPoolBalances = async ({ expectedBalances, given, should }) => {
        const poolTable = await eos.getTableRows({
            code: pool,
            scope: pool,
            table: 'balances',
            json: true,
        })
        assert({
            given,
            should,
            actual: poolTable.rows,
            expected: expectedBalances
        })
    }

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
    
    console.log('create locks')
    await contracts.token.transfer(firstuser, escrow, '100.0000 SEEDS', '', { authorization: `${firstuser}@active` })
    const vesting_date_future = moment().utc().add(1000, 's').toString()
    await Promise.all(
        users
        .slice(1)
        .map((user, index) => 
            contracts.escrow.lock(
                'event', 
                firstuser, 
                user, 
                `${10 * (index+1)}.0000 SEEDS`, 
                golive, 
                hyphadao, 
                vesting_date_future, 
                'notes', 
                { authorization: `${firstuser}@active` }
            )
        )
    )

    await contracts.escrow.lock('event', firstuser, seconduser, `4.0000 SEEDS`, golive, hyphadao, vesting_date_future, 'notes', { authorization: `${firstuser}@active` })
    await contracts.escrow.lock('event', firstuser, thirduser, `4.0000 SEEDS`, 'testevent', firstuser, vesting_date_future, 'notes', { authorization: `${firstuser}@active` })

    await checkLocks({
        expectedLocks: [
            { lock_type: 'event', sponsor: firstuser, beneficiary: seconduser, quantity: '10.0000 SEEDS', trigger_event: golive, trigger_source: hyphadao },
            { lock_type: 'event', sponsor: firstuser, beneficiary: seconduser, quantity: '4.0000 SEEDS', trigger_event: golive, trigger_source: hyphadao },
            { lock_type: 'event', sponsor: firstuser, beneficiary: thirduser, quantity: '20.0000 SEEDS', trigger_event: golive, trigger_source: hyphadao },
            { lock_type: 'event', sponsor: firstuser, beneficiary: thirduser, quantity: '4.0000 SEEDS', trigger_event: 'testevent', trigger_source: firstuser },
            { lock_type: 'event', sponsor: firstuser, beneficiary: fourthuser, quantity: '30.0000 SEEDS', trigger_event: golive, trigger_source: hyphadao }
        ],
        given: 'locks created',
        should: 'have the correct entries'
    })

    console.log('set batchsize to 2')
    await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })
    
    console.log('trigger event golive')
    await contracts.escrow.resettrigger(hyphadao, { authorization: `${escrow}@active` })
    await contracts.escrow.triggertest(hyphadao, golive, 'event notes', { authorization: `${escrow}@active` })
    await sleep(5000)

    await checkLocks({
        expectedLocks: [
            { lock_type: 'event', sponsor: firstuser, beneficiary: thirduser, quantity: '4.0000 SEEDS', trigger_event: 'testevent', trigger_source: firstuser }
        ],
        given: `${golive} event triggered`,
        should: `send dSeeds to ${pool}`
    })

    await checkPoolBalances({
        expectedBalances: [
            { account: seconduser, balance: '14.0000 SEEDS' },
            { account: thirduser, balance: '20.0000 SEEDS' },
            { account: fourthuser, balance: '30.0000 SEEDS' }
        ],
        given: `${golive} event triggered`,
        should: 'have the correct pool balances'
    })

    console.log('add lock after event triggered')
    await contracts.escrow.lock('event', firstuser, thirduser, `2.0000 SEEDS`, golive, hyphadao, vesting_date_future, 'notes', { authorization: `${firstuser}@active` })

    console.log('trigger event testevent')
    await contracts.escrow.resettrigger(firstuser, { authorization: `${escrow}@active` })
    await contracts.escrow.triggertest(firstuser, 'testevent', 'event notes', { authorization: `${escrow}@active` })
    await sleep(1000)

    console.log('claim locks')
    await contracts.escrow.claim(thirduser, { authorization: `${thirduser}@active` })

    await checkLocks({
        expectedLocks: [],
        given: 'all the locks claimed',
        should: `be empty`
    })

    await checkPoolBalances({
        expectedBalances: [
            { account: seconduser, balance: '14.0000 SEEDS' },
            { account: thirduser, balance: '22.0000 SEEDS' },
            { account: fourthuser, balance: '30.0000 SEEDS' }
        ],
        given: 'locks claimed',
        should: 'have the correct pool balances'
    })

})
*/
