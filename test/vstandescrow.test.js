const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("../scripts/helper")
const { equals } = require("ramda")
const { vstandescrow, accounts, token, firstuser, seconduser, thirduser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

describe('vest and escrow', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await Promise.all([
        eos.contract(vstandescrow),
        eos.contract(accounts),
        eos.contract(token),
    ]).then(([vstandescrow, accounts, token]) => ({
        vstandescrow, accounts, token
    }))


    console.log('vstandescrow reset')
    await contracts.vstandescrow.reset({ authorization: `${vstandescrow}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', { authorization: `${accounts}@active` }) 

    console.log('create balance')
    await contracts.token.transfer(firstuser, vstandescrow, "150.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, vstandescrow, "50.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const initialBalances = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'sponsors',
        json: true
    })

    const now = new Date()  
    const secondsSinceEpoch = Math.round(now.getTime() / 1000)
    const vesting_date_passed = secondsSinceEpoch - 100
    const vesting_date_future = secondsSinceEpoch + 1000
    
    console.log('create escrow')
    await contracts.vstandescrow.create(firstuser, seconduser, '20.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })
    await contracts.vstandescrow.createescrow(seconduser, firstuser, '50.0000 SEEDS', vesting_date_future, { authorization: `${seconduser}@active` })

    const initialEscrows = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'escrow',
        json: true
    })

    try {
        console.log('creating an escrow without enough balance')
        await contracts.vstandescrow.create(firstuser, seconduser, '200.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })
    }
    catch(err) {
        const e = JSON.parse(err)
        console.log(e.error.details[0].message.replace('assertion failure with message: ', ''))
    }

    try {
        console.log('claiming an escrow before the vesting date')
        await contracts.vstandescrow.claim(firstuser, { authorization: `${firstuser}@active` })
    }
    catch(err) {
        const e = JSON.parse(err)
        console.log(e.error.details[0].message.replace('assertion failure with message: ', ''))
    }

    const secondUserBalanceBefore = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    console.log('claim escrow')
    await contracts.vstandescrow.claim(seconduser, { authorization: `${seconduser}@active` })

    const escrows = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'escrow',
        json: true
    })

    const secondUserBalanceAfter = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    const middleBalances = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'sponsors',
        json: true
    })

    console.log('cancel escrow')
    await contracts.vstandescrow.cancelescrow(seconduser, firstuser, { authorization: `${seconduser}@active` })

    const escrowsAfter = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'escrow',
        json: true
    })

    const lastBalances = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'sponsors',
        json: true
    })

    const secondUserBalanceBeforewithdraw = await getTableRows({
        code: token,
        scope: seconduser,
        table: 'accounts',
        json: true
    })

    console.log('withdraw')
    await contracts.vstandescrow.withdraw(seconduser, '30.0000 SEEDS', { authorization: `${seconduser}@active` })

    const withdrawBalances = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
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
    await contracts.vstandescrow.create(firstuser, seconduser, '1.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, seconduser, '1.0000 SEEDS', vesting_date_future, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, thirduser, '1.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, thirduser, '1.0000 SEEDS', vesting_date_future, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, seconduser, '1.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, seconduser, '1.0000 SEEDS', vesting_date_future, { authorization: `${firstuser}@active` })
    await sleep(50)
    await contracts.vstandescrow.create(firstuser, seconduser, '1.0000 SEEDS', vesting_date_passed, { authorization: `${firstuser}@active` })

    await contracts.vstandescrow.claim(seconduser, { authorization: `${seconduser}@active` })

    const severalEscrowsAfter = await getTableRows({
        code: vstandescrow,
        scope: vstandescrow,
        table: 'escrow',
        json: true
    })


    assert({
        given: 'the first and second user have transfered tokens to the vest and escrow contract',
        should: 'create the sponsors entries in the sponsors table',
        actual: initialBalances.rows.map(row => row),
        expected: [
            { sponsor: 'escrow.seeds', balance: '200.0000 SEEDS' },
            { sponsor: 'seedsuseraaa', balance: '150.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', balance: '50.0000 SEEDS' }
        ]
    })

    assert({
        given: 'the initial escrows',
        shoule: 'create the correspondent entries in the escrows table',
        actual: initialEscrows.rows.map( row => row),
        expected: [
            {
                id: 0,
                sponsor: 'seedsuseraaa',
                beneficiary: 'seedsuserbbb',
                quantity: '20.0000 SEEDS',
                vesting_date: vesting_date_passed,
                type: 0
            },
            {
                id: 1,
                sponsor: 'seedsuserbbb',
                beneficiary: 'seedsuseraaa',
                quantity: '50.0000 SEEDS',
                vesting_date: vesting_date_future,
                type: 1
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
        actual: escrows.rows.map(row => row),
        expected: [
            {
                id: 1,
                sponsor: 'seedsuserbbb',
                beneficiary: 'seedsuseraaa',
                quantity: '50.0000 SEEDS',
                vesting_date: vesting_date_future,
                type: 1
            }
        ]
    })

    assert({
        given: 'before the escrow was canceled',
        should: 'get the sponsors balances correct',
        actual: middleBalances.rows.map(row => row),
        expected: [
            { sponsor: 'escrow.seeds', balance: '180.0000 SEEDS' },
            { sponsor: 'seedsuseraaa', balance: '130.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', balance: '0.0000 SEEDS' }
        ]
    })

    assert({
        given: 'after the escrow was canceled',
        should: 'update the sponsors balances correctly',
        actual: lastBalances.rows.map(row => row),
        expected: [
            { sponsor: 'escrow.seeds', balance: '180.0000 SEEDS' },
            { sponsor: 'seedsuseraaa', balance: '130.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', balance: '50.0000 SEEDS' }
        ]
    })

    assert({
        given: 'after the escrow was canceled',
        should: 'erase the escrow table entry',
        actual: escrowsAfter.rows.map(row => row),
        expected: []
    })

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
            { sponsor: 'escrow.seeds', balance: '150.0000 SEEDS' },
            { sponsor: 'seedsuseraaa', balance: '130.0000 SEEDS' },
            { sponsor: 'seedsuserbbb', balance: '20.0000 SEEDS' }
        ]
    })

    assert({
        given: 'several escrows created',
        should: 'claim all available escrows',
        actual: severalEscrowsAfter.rows.map(row => row),
        expected: [
            {
                id: 1,
                sponsor: 'seedsuseraaa',
                beneficiary: 'seedsuserbbb',
                quantity: '1.0000 SEEDS',
                vesting_date: vesting_date_future,
                type: 0
            },
            {
                id: 2,
                sponsor: 'seedsuseraaa',
                beneficiary: 'seedsuserccc',
                quantity: '1.0000 SEEDS',
                vesting_date: vesting_date_passed,
                type: 0
            },
            {
                id: 3,
                sponsor: 'seedsuseraaa',
                beneficiary: 'seedsuserccc',
                quantity: '1.0000 SEEDS',
                vesting_date: vesting_date_future,
                type: 0
            },
            {
                id: 5,
                sponsor: 'seedsuseraaa',
                beneficiary: 'seedsuserbbb',
                quantity: '1.0000 SEEDS',
                vesting_date: vesting_date_future,
                type: 0
            }
        ]
    })
})


