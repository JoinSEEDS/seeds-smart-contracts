const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("../scripts/helper")
const { equals } = require("ramda")

const { organization, accounts, token, firstuser, seconduser, thirduser, bank, settings, history } = names

describe('organization', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await Promise.all([
        eos.contract(organization),
        eos.contract(token),
        eos.contract(accounts),
        eos.contract(settings),
    ]).then(([organization, token, accounts, settings]) => ({
        organization, token, accounts, settings
    }))

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('reset token stats')
    await contracts.token.resetstats({ authorization: `${token}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('configure')
    await contracts.settings.configure('fee', 500000, { authorization: `${settings}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', { authorization: `${accounts}@active` })

    console.log('add rep')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 13000, { authorization: `${accounts}@active` })

    console.log('create balance')
    await contracts.token.transfer(firstuser, organization, "150.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, organization, "50.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const initialBalances = await getTableRows({
        code: organization,
        scope: organization,
        table: 'balances',
        json: true
    })

    console.log('create organization')
    await contracts.organization.create('testorg1', firstuser, { authorization: `${firstuser}@active` })
    await contracts.organization.create('testorg2', firstuser, { authorization: `${firstuser}@active` })
    await contracts.organization.create('testorg3', seconduser, { authorization: `${seconduser}@active` })

    const initialOrgs = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })

    console.log('add member')
    await contracts.organization.addmember('testorg1', firstuser, seconduser, 'admin', { authorization: `${firstuser}@active` })
    await contracts.organization.addmember('testorg3', seconduser, firstuser, 'admin', { authorization: `${seconduser}@active` })
    

    const members1 = await getTableRows({
        code: organization,
        scope: 'testorg1',
        table: 'members',
        json: true
    })

    const members2 = await getTableRows({
        code: organization,
        scope: 'testorg3',
        table: 'members',
        json: true
    })

    console.log('destroy organization')
    await contracts.organization.destroy('testorg2', firstuser, { authorization: `${firstuser}@active` })
    await contracts.organization.refund(firstuser, "50.0000 SEEDS", { authorization: `${firstuser}@active` })

    const orgs = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })

    console.log('change owner')

    try{
        console.log('remove owner')
        await contracts.organization.removemember('testorg3', firstuser, firstuser, { authorization: `${firstuser}@active` })
    }
    catch(err){
        console.log('You can not remove de owner')
    }

    await contracts.organization.changeowner('testorg3', seconduser, firstuser, { authorization: `${seconduser}@active` })
    await contracts.organization.changerole('testorg3', firstuser, seconduser, 'testrole', { authorization: `${firstuser}@active` })

    const currentRoles = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })

    console.log('remove member')
    await contracts.organization.removemember('testorg3', firstuser, seconduser, { authorization: `${firstuser}@active` })

    const members3 = await getTableRows({
        code: organization,
        scope: 'testorg3',
        table: 'members',
        json: true
    })

    console.log('add regen')
    await contracts.organization.addregen('testorg1', firstuser, { authorization: `${firstuser}@active` })
    await contracts.organization.subregen('testorg3', seconduser, { authorization: `${seconduser}@active` })
    
    const regen = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })


    try{
        console.log('create organization')
        await contracts.organization.create('testorg4', thirduser, { authorization: `${thirduser}@active`  })
    }
    catch(err){
        console.log('user thoes not have a balance entry')
    }

    try{
        console.log('create organization')
        await contracts.token.transfer(thirduser, organization, "20.0000 SEEDS", "Initial supply", { authorization: `${thirduser}@active` })
        await contracts.organization.create('testorg4', thirduser, { authorization: `${thirduser}@active`  })
    }
    catch(err){
        console.log('user has not enough balance')
    }



    assert({
        given: 'firstuser and second user transfer to organization contract',
        should: 'update the balances table',
        actual: initialBalances.rows.map(row => { return row }),
        expected: [
            {
                account: 'orgs.seeds',
                balance: '200.0000 SEEDS'
            },
            {
                account: 'seedsuseraaa',
                balance: '150.0000 SEEDS'
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

    assert({
        given: 'Orgs having members',
        should: 'Change roles properly',
        actual: members1.rows.map(row => {
            return row
        }),
        expected: [
            {
                account: 'seedsuseraaa',
                role: ''
            },
            {
                account: 'seedsuserbbb',
                role: 'admin'
            }
        ]
    })

    assert({
        given: 'Orgs having members',
        should: 'Change roles properly',
        actual: members2.rows.map(row => {
            return row
        }),
        expected: [
            {
                account: 'seedsuseraaa',
                role: 'admin'
            },
            {
                account: 'seedsuserbbb',
                role: ''
            }
        ]
    })

    assert({
        given: 'Organization destroyed and the refund function called',
        should: 'erase the organization its members and give the funds back to the user',
        actual: orgs.rows.map(row => {
            return row
        }),
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

    assert({
        given: 'Roles changed',
        should: 'Update the organization information',
        actual: currentRoles.rows.map(row => {return row}),
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
                org_name: 'testorg3',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            }
        ]
    })

    assert({
        given: 'A memeber removed',
        should: 'Erase the member',
        actual: members3.rows.map(row => {
            return row
        }),
        expected: [
            {
                account: 'seedsuseraaa',
                role: 'admin'
            }
        ]
    })

    assert({
        given: 'Users voted',
        should: 'Update the regen points according to users\' rep',
        actual: regen.rows.map(row => {
            return row
        }),
        expected: [
            {
                org_name: 'testorg1',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 10000,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuseraaa',
                status: 0,
                regen: -13000,
                reputation: 0,
                voice: 0,
                fee: "50.0000 SEEDS"
            }
        ]
    })
})








