const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("../scripts/helper")
const { equals } = require("ramda")
const { escrow, accounts, token, firstuser, seconduser, thirduser } = names
const moment = require('moment');

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

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

    console.log('withdraw')
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
        actual: secondUserBalanceAfter.rows.map(row => { return { balance: parseFloat(row.balance.replace(' SEEDS')) } }),
        expected: [{
            balance: parseFloat(secondUserBalanceBefore.rows[0].balance.replace(' SEEDS', '')) + 20
        }]
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
        actual: secondUserBalanceAfterwithdraw.rows.map(row => {
            return {
                balance: parseFloat(row.balance.replace(' SEEDS'))
            }
        }),
        expected: secondUserBalanceBeforewithdraw.rows.map(row => { 
            return {
                balance: parseFloat(row.balance.replace(' SEEDS')) + 30
            } 
        })
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
