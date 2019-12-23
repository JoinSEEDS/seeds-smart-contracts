const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("../scripts/helper")
const { equals } = require("ramda")

const { orgtns, accounts, token, firstuser, seconduser, bank, settings, history } = names

describe('organization', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await Promise.all([
        eos.contract(orgtns),
        eos.contract(token),
        eos.contract(accounts),
        eos.contract(settings),
    ]).then(([orgtns, token, accounts, settings]) => ({
        orgtns, token, accounts, settings
    }))

    console.log('reset orgtns')
    await contracts.orgtns.reset({ authorization: `${orgtns}@active` })

    console.log('reset token stats')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('configure')
    await contracts.settings.configure('fee', 500000, { authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', { authorization: `${accounts}@active` })

    console.log('create balance')
    await contracts.token.transfer(firstuser, orgtns, "100.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, orgtns, "50.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const initialBalances = await getTableRows({
        code: orgtns,
        scope: orgtns,
        table: 'balances',
        json: true
    })

    console.log('create organization')
    await contracts.orgtns.create('testorg1', firstuser, { authorization: `${firstuser}@active` })
    await contracts.orgtns.create('testorg2', firstuser, { authorization: `${firstuser}@active` })
    await contracts.orgtns.create('testorg3', seconduser, { authorization: `${seconduser}@active` })

    const initialOrgs = await getTableRows({
        code: orgtns,
        scope: orgtns,
        table: 'organization',
        json: true
    })

    /*
    console.log('add member')
    await contracts.orgtns.addmember('testorg1', firstuser, seconduser, 'admin', { authorization: `${firstuser}@active` })
    await contracts.orgtns.addmember('testorg3', seconduser, firstuser, 'admin', { authorization: `${seconduser}@active` })
    */

    
    console.log('destroy organization')
    await contracts.orgtns.destroy('testorg2', firstuser, { authorization: `${firstuser}@active` })
/*
    console.log('change owner')
    await contracts.orgtns.changeowner('testorg3', seconduser, firstuser, { authorization: `${seconduser}@active` })
    await contracts.orgtns.changerole('testorg3', firstuser, seconduser, 'testrole', { authorization: `${firstuser}@active` })

    console.log('remove member')
    await contracts.orgtns.removemember('testorg3', firstuser, seconduser, { authorization: `${firstuser}@active` })

    console.log('refund')
    await contracts.orgtns.refund(seconduser, "50.0000 SEEDS", { authorization: `${seconduser}@active` })

    console.log('add regen')
    await contracts.orgtns.addregen('testorg1', firstuser, { authorization: `${firstuser}@active` })
    await contracts.orgtns.subregen('testorg3', firstuser, { authorization: `${firstuser}@active` })
    */

    assert({
        given: 'firstuser and second user transfer to orgtns contract',
        should: 'update the balances table',
        actual: initialBalances.rows.map(row => { return row }),
        expected: [
            {
                account: 'orgtns.seeds',
                balance: '150.0000 SEEDS'
            },
            {
                account: 'seedsuseraaa',
                balance: '100.0000 SEEDS'
            },
            {
                account: 'seedsuserbbb',
                balance: '50.0000 SEEDS'
            },
        ]
    })


    assert({
        given: 'organizations created',
        should: 'update the organizations table',
        actual: initialOrgs.rows.map(row => { return row }),
        expected: [
            {
                org_name: 'testorg1',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            },
            {
                org_name: 'testorg2',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuserbbb',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            }
        ]
    })


})








