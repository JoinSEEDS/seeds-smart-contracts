const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts, ramdom64ByteHexString, sha256, fromHexString } = require("../scripts/helper")
const { equals, any, or } = require("ramda")

const { organization, accounts, token, firstuser, seconduser, thirduser, fourthuser, fifthuser, bank, settings, harvest, history, exchange, onboarding } = names

let eosDevKey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

function getBeginningOfDayInSeconds () {
    const now = new Date()
    return now.setUTCHours(0, 0, 0, 0) / 1000
}  

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

const randomAccountName = () => {
    let length = 12
    var result           = '';
    var characters       = 'abcdefghijklmnopqrstuvwxyz1234';
    var charactersLength = characters.length;
    for ( var i = 0; i < length; i++ ) {
       result += characters.charAt(Math.floor(Math.random() * charactersLength));
    }
    return result;
}

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
        eos.contract(harvest),
    ]).then(([organization, token, accounts, settings, harvest]) => ({
        organization, token, accounts, settings, harvest
    }))
    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('reset token stats')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })
    
    console.log('harvest reset')
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    // console.log('configure')
    // await contracts.settings.configure('planted', 500000, { authorization: `${settings}@active` })
  
    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

    console.log('add rep')
    await contracts.accounts.testsetrs(firstuser, 49, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(seconduser, 99, { authorization: `${accounts}@active` })

    console.log('create balance')
    await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    const initialBalances = await getTableRows({
        code: organization,
        scope: organization,
        table: 'sponsors',
        json: true
    })

    console.log('create organization')
    await contracts.organization.create(firstuser, 'testorg1', "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.create(firstuser, 'testorg2', "Org 2", eosDevKey,  { authorization: `${firstuser}@active` })
    await contracts.organization.create(seconduser, 'testorg3', "Org 3 - Test, Inc.", eosDevKey, { authorization: `${seconduser}@active` })

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
    //await contracts.organization.refund(firstuser, "50.0000 SEEDS", { authorization: `${firstuser}@active` })

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
    await contracts.organization.addregen('testorg1', firstuser, 15, { authorization: `${firstuser}@active` })
    await contracts.organization.subregen('testorg3', seconduser, 1, { authorization: `${seconduser}@active` })
    
    const regen = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })


    try{
        console.log('create organization')
        await contracts.organization.create(thirduser, 'testorg4', eosDevKey, { authorization: `${thirduser}@active`  })
    }
    catch(err){
        console.log('user thoes not have a balance entry')
    }

    try{
        console.log('create organization')
        await contracts.token.transfer(thirduser, organization, "20.0000 SEEDS", "Initial supply", { authorization: `${thirduser}@active` })
        await contracts.organization.create(thirduser, 'testorg4', eosDevKey, { authorization: `${thirduser}@active`  })
    }
    catch(err){
        console.log('user has not enough balance')
    }

    assert({
        given: 'firstuser and second user transfer to organization contract',
        should: 'update the sponsors table',
        actual: initialBalances.rows.map(row => { return row }),
        expected: [
            {
                account: 'orgs.seeds',
                balance: '600.0000 SEEDS'
            },
            {
                account: 'seedsuseraaa',
                balance: '400.0000 SEEDS'
            },
            {
                account: 'seedsuserbbb',
                balance: '200.0000 SEEDS'
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
                planted: "200.0000 SEEDS"
            },
            {
                org_name: 'testorg2',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuserbbb',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
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
                planted: "200.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuserbbb',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
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
                planted: "200.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuseraaa',
                status: 0,
                regen: 0,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
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
                regen: 6,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
            },
            {
                org_name: 'testorg3',
                owner: 'seedsuseraaa',
                status: 0,
                regen: -2,
                reputation: 0,
                voice: 0,
                planted: "200.0000 SEEDS"
            }
        ]
    })
})


describe('app', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await Promise.all([
        eos.contract(organization),
        eos.contract(token),
        eos.contract(accounts),
        eos.contract(settings),
        eos.contract(harvest),
    ]).then(([organization, token, accounts, settings, harvest]) => ({
        organization, token, accounts, settings, harvest
    }))

    console.log('changing batch size')
    await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
    
    console.log('create organization')
    await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })
    await contracts.organization.create(firstuser, 'testorg1', "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.create(seconduser, 'testorg2', "Org Number 2", eosDevKey, { authorization: `${seconduser}@active` })

    console.log('register app')
    await contracts.organization.registerapp(firstuser, 'testorg1', 'app1', 'app long name', { authorization: `${firstuser}@active` })
    await contracts.organization.registerapp(seconduser, 'testorg2', 'app2', 'app long name 2', { authorization: `${seconduser}@active` })

    let createOrgNotBeingOwner = true
    try {
        await contracts.organization.registerapp(seconduser, 'testorg1', 'app3', 'app3 long name', { authorization: `${seconduser}@active` })
        console.log('app3 registered (not expected)')
    } catch (err) {
        createOrgNotBeingOwner = false
        console.log('only the owner can register an app (expected)')
    }

    console.log('use app')
    await contracts.organization.appuse('app1', firstuser, { authorization: `${firstuser}@active` })
    await sleep(300)
    
    for (let i = 0; i < 10; i++) {
        await contracts.organization.appuse('app1', seconduser, { authorization: `${seconduser}@active` })
        await sleep(400)
    }

    await contracts.organization.appuse('app2', seconduser, { authorization: `${seconduser}@active` })

    const daus1Table = await getTableRows({
        code: organization,
        scope: 'app1',
        table: 'daus',
        json: true
    })

    const daus2Table = await getTableRows({
        code: organization,
        scope: 'app2',
        table: 'daus',
        json: true
    })

    const appsTable = await getTableRows({
        code: organization,
        scope: organization,
        table: 'apps',
        json: true
    })
    const apps = appsTable.rows

    console.log('clean daus today')
    await contracts.organization.cleandaus({ authorization: `${organization}@active` })
    await sleep(3000)

    const daus1TableAfterClean1 = await getTableRows({
        code: organization,
        scope: 'app1',
        table: 'daus',
        json: true
    })

    const daus2TableAfterClean1 = await getTableRows({
        code: organization,
        scope: 'app2',
        table: 'daus',
        json: true
    })

    let today = new Date()
    today.setUTCHours(0, 0, 0, 0)
    today = today.getTime() / 1000

    let tomorrow = new Date()
    tomorrow.setUTCHours(0, 0, 0, 0)
    tomorrow.setDate(tomorrow.getDate() + 1)
    tomorrow = tomorrow.getTime() / 1000

    console.log('clean app1 dau')
    await contracts.organization.cleandau('app1', tomorrow, 0, { authorization: `${organization}@active` })
    await sleep(2000)

    const daus1TableAfterClean2 = await getTableRows({
        code: organization,
        scope: 'app1',
        table: 'daus',
        json: true
    })

    const daus1History1 = await getTableRows({
        code: organization,
        scope: 'app1',
        table: 'dauhistory',
        json: true
    })

    console.log('ban app')
    await contracts.organization.banapp('app1', { authorization: `${organization}@active` })

    const appsTableAfterBan = await getTableRows({
        code: organization,
        scope: organization,
        table: 'apps',
        json: true
    })

    console.log('reset settings')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    assert({
        given: 'registered an app',
        should: 'have an entry in the apps table',
        actual: apps,
        expected: [
            { 
                app_name: 'app1',
                org_name: 'testorg1',
                app_long_name: 'app long name',
                is_banned: 0,
                number_of_uses: 11
            },
            { 
                app_name: 'app2',
                org_name: 'testorg2',
                app_long_name: 'app long name 2',
                is_banned: 0,
                number_of_uses: 1
            }
        ]
    })

    assert({
        given: 'appuse called',
        should: 'increment the app use counter',
        actual: [
            daus1Table.rows.map(values => values.number_app_uses),
            daus2Table.rows.map(values => values.number_app_uses)
        ],
        expected: [[1, 10], [1]]
    })

    assert({
        given: 'register app by not the owner',
        should: 'not register the app',
        actual: createOrgNotBeingOwner,
        expected: false
    })

    assert({
        given: 'call cleandaus on the same day',
        should: 'not clean daus app1 table',
        actual: daus1TableAfterClean1.rows,
        expected: daus1Table.rows
    })

    assert({
        given: 'call cleandaus on the same day',
        should: 'not clean daus app2 table',
        actual: daus2TableAfterClean1.rows,
        expected: daus2Table.rows
    })

    assert({
        given: 'call cleandau for app1 on a different day',
        should: 'clean daus app1 table',
        actual: daus1TableAfterClean2.rows,
        expected: [ 
            { account: 'seedsuseraaa', date: tomorrow, number_app_uses: 0 },
            { account: 'seedsuserbbb', date: tomorrow, number_app_uses: 0 } 
        ]
    })

    assert({
        given: 'call cleandau for app1 on a different day',
        should: 'have entries in the dau history table',
        actual: daus1History1.rows,
        expected: [ 
            { 
                dau_history_id: 0,
                account: 'seedsuseraaa',
                date: today,
                number_app_uses: 1 
            },
            { 
                dau_history_id: 1,
                account: 'seedsuserbbb',
                date: today,
                number_app_uses: 10 
            } 
        ]
    })

    assert({
        given: 'ban app called',
        should: 'ban the app',
        actual: appsTableAfterBan.rows,
        expected: [
            { 
                app_name: 'app1',
                org_name: 'testorg1',
                app_long_name: 'app long name',
                is_banned: 1,
                number_of_uses: 11
            },
            { 
                app_name: 'app2',
                org_name: 'testorg2',
                app_long_name: 'app long name 2',
                is_banned: 0,
                number_of_uses: 1
            }
        ]
    })

})

describe('organization scores', async assert => {
    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await Promise.all([
        eos.contract(organization),
        eos.contract(token),
        eos.contract(accounts),
        eos.contract(settings),
        eos.contract(harvest),
        eos.contract(history)
    ]).then(([organization, token, accounts, settings, harvest, history]) => ({
        organization, token, accounts, settings, harvest, history
    }))
    
    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })
    await contracts.settings.configure('orgratethrsh', 0, { authorization: `${settings}@active` })

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('reset token stats')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })
    
    console.log('harvest reset')
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    const org1 = 'testorg1'
    const org2 = 'testorg2'
    const org3 = 'testorg3'
    const org4 = 'testorg4'
    const org5 = 'testorg5'

    const orgs = [ org1, org2, org3, org4, org5 ]
    const users1 = [ firstuser, seconduser, thirduser ]
    
    orgs.map(async org => { await contracts.history.reset(org, { authorization: `${history}@active` }) })
    users1.map( async user => { await contracts.history.reset(user, { authorization: `${history}@active` }) })

    const day = getBeginningOfDayInSeconds()
    await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(thirduser, 'third user', 'individual', { authorization: `${accounts}@active` })

    console.log('add rep')
    await contracts.accounts.testsetrs(firstuser, 99, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(seconduser, 66, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(thirduser, 33, { authorization: `${accounts}@active` })
    
    console.log('create balance')
    await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, organization, "600.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })

    console.log('create organizations')
    await contracts.organization.create(firstuser, org1, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.create(firstuser, org2, "Org 2", eosDevKey,  { authorization: `${firstuser}@active` })
    await contracts.organization.create(seconduser, org3, "Org 3 - Test, Inc.", eosDevKey, { authorization: `${seconduser}@active` })
    await contracts.organization.create(seconduser, org4, "Org 4 - Test, Inc.", eosDevKey, { authorization: `${seconduser}@active` })
    await contracts.organization.create(seconduser, org5, "Org 5 - Test, Inc.", eosDevKey, { authorization: `${seconduser}@active` })

    console.log('modify regen')
    await contracts.organization.subregen(org1, firstuser, 1, { authorization: `${firstuser}@active` })
    await contracts.organization.subregen(org1, seconduser, 1, { authorization: `${seconduser}@active` })

    await contracts.organization.addregen(org2, firstuser, 4, { authorization: `${firstuser}@active` })
    await contracts.organization.addregen(org2, seconduser, 3, { authorization: `${seconduser}@active` })

    await contracts.organization.addregen(org3, firstuser, 6, { authorization: `${firstuser}@active` })
    await contracts.organization.subregen(org3, seconduser, 6, { authorization: `${seconduser}@active` })
    await contracts.organization.subregen(org3, thirduser, 6, { authorization: `${thirduser}@active` })

    await contracts.organization.addregen(org4, firstuser, 7, { authorization: `${firstuser}@active` })
    await contracts.organization.addregen(org4, seconduser, 5, { authorization: `${seconduser}@active` })

    await contracts.organization.addregen(org5, firstuser, 12, { authorization: `${firstuser}@active` })
    await contracts.organization.addregen(org5, seconduser, 12, { authorization: `${seconduser}@active` })
    
    await contracts.accounts.testsetcbs(org1, 100, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetcbs(org2, 75, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetcbs(org3, 50, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetcbs(org4, 25, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetcbs(org5, 1, { authorization: `${accounts}@active` })

    Promise.all(orgs.map(org => contracts.accounts.testsetrs(org, 49, { authorization: `${accounts}@active` })))

    await contracts.accounts.rankorgreps({ authorization: `${accounts}@active` })
    await sleep(200)

    console.log('transfer volume of seeds')
    const transfer = async (from, to, amount) => {
        await contracts.token.transfer(from, to, `${amount}.0000 SEEDS`, "Test supply", { authorization: `${from}@active` })
        await sleep(3000)
    }

    await transfer(firstuser, org1, 150)
    await transfer(org1, firstuser, 50)
    await transfer(org1, seconduser, 50)
    await transfer(org1, thirduser, 50)

    await transfer(firstuser, org2, 300)
    await transfer(org2, firstuser, 100)
    await transfer(org2, seconduser, 100)
    await transfer(org2, thirduser, 100)

    await transfer(firstuser, org3, 600)
    await transfer(org3, firstuser, 200)
    await transfer(org3, seconduser, 200)
    await transfer(org3, thirduser, 200)

    await transfer(firstuser, org4, 800)
    await transfer(org4, firstuser, 200)
    await transfer(org4, seconduser, 300)
    await transfer(org4, thirduser, 300)

    await transfer(seconduser, org5, 1000)
    await transfer(org5, firstuser, 600)
    await transfer(org5, seconduser, 200)
    await transfer(org5, thirduser, 200)

    await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })
    await sleep(100)

    await contracts.organization.rankregens({ authorization: `${organization}@active` })
    await contracts.accounts.rankorgcbss({ authorization: `${accounts}@active` })
    await contracts.harvest.rankorgtxs({ authorization: `${harvest}@active` })

    await sleep(5000)

    const orgTxScore = await getTableRows({
        code: harvest,
        scope: "org",
        table: 'txpoints',
        json: true
    })
    console.log('orgTxScore:', orgTxScore)

    const orgsTable = await getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true
    })
    console.log(orgsTable)

    const regenScores = await getTableRows({
        code: organization,
        scope: organization,
        table: 'regenscores',
        json: true
    })
    console.log('Regen Scores:', regenScores)

    const cbsRanks = await getTableRows({
        code: accounts,
        scope: 'org',
        table: 'cbs',
        json: true
    })

    const txRanks = await getTableRows({
        code: harvest,
        scope: "org",
        table: 'txpoints',
        json: true
    })

    const avgsBefore = await getTableRows({
        code: organization,
        scope: organization,
        table: 'avgvotes',
        json: true
    })

    await contracts.organization.addregen(org1, firstuser, 10, { authorization: `${firstuser}@active` })
    
    const avgsAfter = await getTableRows({
        code: organization,
        scope: organization,
        table: 'avgvotes',
        json: true
    })

    const getTransactionPoints = async (users) => {
        const trx = []
        for (const user of users) {
            const trxPoints = await getTableRows({
                code: history,
                scope: user,
                table: 'trxpoints',
                json: true
            })
            trx.push(trxPoints.rows)
        }
        return trx
    }

    console.log(await getTransactionPoints([org1, org2, org3, org4, org5]))


    assert({
        given: 'regen scores calculated',
        should: 'rank the orgs properly',
        actual: regenScores.rows,
        expected: [
            { org_name: org2, regen_avg: 66000, rank: 27 },
            { org_name: org3, regen_avg: 2000, rank: 0 },
            { org_name: org4, regen_avg: 33000, rank: 4 },
            { org_name: org5, regen_avg: 115500, rank: 63 }
        ]
    })

    assert({
        given: 'cbs calculated',
        should: 'rank the orgs properly',
        actual: cbsRanks.rows,
        expected: [
            { org_name: org1, community_building_score: 100, rank: 71 },
            { org_name: org2, community_building_score: 75, rank: 41 },
            { org_name: org3, community_building_score: 50, rank: 14 },
            { org_name: org4, community_building_score: 25, rank: 2 },
            { org_name: org5, community_building_score: 1, rank: 0 }
        ]
    })

    // console.log('cbs', cbsRanks.rows)

    assert({
        given: 'txs calculated',
        should: 'rank thr orgs properly',
        actual: txRanks.rows,
        expected: [
            { account: org1, points: 212, rank: 0 },
            { account: org2, points: 423, rank: 2 },
            { account: org3, points: 843, rank: 41 },
            { account: org4, points: 1061, rank: 71 },
            { account: org5, points: 802, rank: 14 }
        ]
    })
    console.log('trxs', txRanks.rows)

    assert({
        given: 'users voted',
        should: 'have the correct average',
        actual: avgsBefore.rows,
        expected: [
            {
                org_name: org1,
                total_sum: -3,
                num_votes: 2,
                average: -1
            },
            {
                org_name: org2,
                total_sum: 12,
                num_votes: 2,
                average: 6
            },
            {
                org_name: org3,
                total_sum: 0,
                num_votes: 3,
                average: 0
            },
            {
                org_name: org4,
                total_sum: 20,
                num_votes: 2,
                average: 10
            },
            {
                org_name: org5,
                total_sum: 23,
                num_votes: 2,
                average: 11
            }
        ]
    })

    assert({
        given: 'users voted',
        should: 'have the correct average',
        actual: avgsAfter.rows,
        expected: [
            {
                org_name: org1,
                total_sum: 13,
                num_votes: 2,
                average: 6
            },
            {
                org_name: org2,
                total_sum: 12,
                num_votes: 2,
                average: 6
            },
            {
                org_name: org3,
                total_sum: 0,
                num_votes: 3,
                average: 0
            },
            {
                org_name: org4,
                total_sum: 20,
                num_votes: 2,
                average: 10
            },
            {
                org_name: org5,
                total_sum: 23,
                num_votes: 2,
                average: 11
            }
        ]
    })

})

describe('organization status', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ organization, settings, harvest, accounts, token, exchange, onboarding, history })

    const checkOrgStatus = async (expectedStatus, n=null) => {
        const orgTable = await getTableRows({
            code: organization,
            scope: organization,
            table: 'organization',
            json: true
        })
        assert({
            given: 'orgs changed status' + (n==null ? '' : ` (index ${n})`),
            should: 'have the correct status',
            actual: orgTable.rows.map(r => r.status),
            expected: expectedStatus
        })
    }

    const minPlanted = 'orgminplnt.'
    const minRepRank = 'orgminrank.'
    const minRegenScore = 'org.rated.'
    const minVisitor = 'org.visref.'
    const minResidents = 'org.resref.'

    const org1 = 'testorg1'
    const org2 = 'testorg2'
    const org3 = 'testorg3'
    const org4 = 'testorg4'
    const org5 = 'testorg5'

    const regular = 0
    const reputable = 1
    const sustainable = 2
    const regenerative = 3
    const thrivable = 4

    const statusStrings = ['regular', 'reputable', 'sustainable', 'regenerative', 'thrivable']

    const orgs = [ org1, org2, org3, org4, org5 ]

    const users = [firstuser, seconduser, thirduser, fourthuser, fifthuser]

    const getSetting = async (key) => {
        const sttgs = await getTableRows({
            code: settings,
            scope: settings,
            table: 'config',
            json: true,
            limit: 10000
        })
        const param = sttgs.rows.filter(r => r.param == key)
        return param[0].value
    }

    const makeStatus = async (org, status, ignore=[]) => {
        if (!ignore.includes(minPlanted)) {
            const mPlanted = (await getSetting(minPlanted + (status+1))) / 10000
            await contracts.token.transfer(firstuser, org, `${mPlanted}.0000 SEEDS`, "for harvest", { authorization: `${firstuser}@active` })
            await contracts.token.transfer(org, harvest, `${mPlanted}.0000 SEEDS`, `sow ${org}`, { authorization: `${org}@active` })
        }
        if (!ignore.includes(minRepRank)) {
            const mRepRank = await getSetting(minRepRank + (status+1))
            await contracts.accounts.testsetrs(org, mRepRank, { authorization: `${accounts}@active` })
        }
        if (!ignore.includes(minRegenScore)) {
            const mRegenScore = await getSetting(minRegenScore + (status+1))
            await contracts.organization.testregensc(org, mRegenScore, { authorization: `${organization}@active` })
        }
        if (!ignore.includes(minVisitor)) {
            const mVisitor = await getSetting(minVisitor + (status+1))
            const refsTable = await getTableRows({
                code: settings,
                scope: settings,
                table: 'config',
                json: true
            })
            const orgRefs = refsTable.rows.filter(r => r.referrer == org)

            for (let i = orgRefs.length; i < mVisitor; i++) {
                const inviteSecret = await ramdom64ByteHexString()
                const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')
                const newAccount = randomAccountName()
                await contracts.token.transfer(seconduser, org, '15.0000 SEEDS', '', { authorization: `${seconduser}@active` })
                await contracts.token.transfer(org, onboarding, '15.0000 SEEDS', '', { authorization: `${org}@active` })
                await contracts.onboarding.invite(org, '10.0000 SEEDS', '5.0000 SEEDS', inviteHash, { authorization: `${org}@active` })
                await contracts.onboarding.accept(newAccount, inviteSecret, eosDevKey, { authorization: `${onboarding}@active` })
            }
        }
        if (!ignore.includes(minResidents)) {
            const mResidents = await getSetting(minResidents + (status+1))
            const refsTable = await getTableRows({
                code: accounts,
                scope: accounts,
                table: 'refs',
                json: true,
                limit: 10000
            })
            const orgRefs = refsTable.rows.filter(r => r.referrer == org).map(r => r.invited)
            const usersTable = await getTableRows({
                code: accounts,
                scope: accounts,
                table: 'users',
                json: true,
                limit: 10000
            })
            const usrs = usersTable.rows.filter(r => orgRefs.includes(r.account))
            usrs.sort((a, b) => {
                if (a.status == 'visitor') {
                    return 1
                }
                return -1
            })
            for (let i=0, j=0; i < usrs.length; i++) {
                if (usrs[i].status == 'visitor') {
                    if (j < mResidents) {
                        await contracts.accounts.testresident(usrs[i].account, { authorization: `${accounts}@active` })
                    }
                }
                j += 1
            }
        }
    }

    const handleError = (wrapped) => {
        return async function() {
            try {
                await wrapped.apply(this, arguments);
                return false
            } catch (err) {
                console.log(err.json.error.details[0])
                return true
            }
        }
    }

    const assertFailure = async ({ given, should, actual }) => {
        assert({
            given,
            should,
            actual,
            expected: true
        })
    }

    const checkHistoryRecords = async (scope, expected) => {
        const historyTables = await getTableRows({
            code: history,
            scope: scope,
            table: 'acctstatus',
            json: true,
            limit: 10000
        })
        const entries = historyTables.rows.map(r => r.account)
        assert({
            given: 'orgs updated status',
            should: 'be in the entries',
            actual: entries,
            expected
        })
    }

    console.log('reset token stats')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('reset exchange')
    await contracts.exchange.reset({ authorization: `${exchange}@active` })
    await contracts.exchange.initrounds( 10 * 10000, "90.9091 SEEDS", { authorization: `${exchange}@active` })

    console.log('reset settings')
    await contracts.settings.reset({ authorization: `${settings}@active` })
    await contracts.settings.configure('txlimit.min', 1000, { authorization: `${settings}@active` })

    console.log('reset accounts')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log('reset harvest')
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('reset onboarding')
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    const regenScores = await getTableRows({
        code: organization,
        scope: organization,
        table: 'regenscores',
        json: true
    })
    console.log(regenScores)

    console.log('make reputable')

    // with Spline ranks the "average" rank is now 27
    await contracts.settings.configure('rep.minrank', 27, { authorization: `${settings}@active` })

    await contracts.organization.makereptable(org2, { authorization: `${organization}@active` })

    await contracts.settings.configure('rgen.resref', 1, { authorization: `${settings}@active` })
    await contracts.settings.configure('rgen.refrred', 3, { authorization: `${settings}@active` })
    // with Spline ranks the "average" rank is now 27
    await contracts.settings.configure('rgen.minrank', 27, { authorization: `${settings}@active` })


    orgs.map(async org => { await contracts.history.reset(org, { authorization: `${history}@active` }) })
    users.map( async user => { await contracts.history.reset(user, { authorization: `${history}@active` }) })

    const day = getBeginningOfDayInSeconds()
    await contracts.history.deldailytrx(day, { authorization: `${history}@active` })

    console.log('join users')
    await Promise.all(users.map(user => contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })))

    console.log('create organizations')
    for (let index = 0; index < orgs.length; index++) {
        const user = users[index%2]
        const org = orgs[index]
        await contracts.token.transfer(user, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${user}@active` })
        await contracts.organization.create(user, org, `Org Number ${index}`, eosDevKey, { authorization: `${user}@active` })
    }

    await checkOrgStatus([regular, regular, regular, regular, regular])

    const makereptableWithError = handleError(contracts.organization.makereptable)
    const makesustnbleWithError = handleError(contracts.organization.makesustnble)
    const makeregenWithError = handleError(contracts.organization.makeregen)
    const makethrivbleWithError = handleError(contracts.organization.makethrivble)

    const ignore = [minPlanted, minRepRank, minRegenScore, minVisitor, minResidents]

    console.log('trying to update status without meeting the requirements')
    for (let i = 0; i < ignore.length-1; i++) {
        await makeStatus(org1, reputable, ignore.slice(i))
        await assertFailure({
            given: `${org1} trying to upgrade to ${statusStrings[reputable]}`,
            should: 'fail',
            actual: await makereptableWithError(org1, { authorization: `${org1}@active` })
        })
    }

    console.log(`make ${org1} ${statusStrings[reputable]}`)
    await makeStatus(org1, reputable)
    await makereptableWithError(org1, { authorization: `${org1}@active` })
    await checkOrgStatus([reputable, regular, regular, regular, regular])
    await checkHistoryRecords(statusStrings[reputable], [org1])

    console.log(`make ${org1} regenerative`)
    await makeStatus(org1, regenerative)
    await assertFailure({
        given: `${org1} trying to upgrade to ${statusStrings[regenerative]}`,
        should: `fail because ${org1} is not ${statusStrings[sustainable]}`,
        actual: await makeregenWithError(org1, { authorization: `${org1}@active` })
    })
    await makesustnbleWithError(org1, { authorization: `${org1}@active` })
    await checkOrgStatus([sustainable, regular, regular, regular, regular])
    await checkHistoryRecords(statusStrings[sustainable], [org1])

    await contracts.organization.makeregen(org1, { authorization: `${org1}@active` })
    await checkOrgStatus([regenerative, regular, regular, regular, regular])
    await checkHistoryRecords(statusStrings[regenerative], [org1])

    console.log()
    for (let i = 0; i < ignore.length-1; i++) {
        await makeStatus(org2, thrivable, ignore.slice(i))
        await assertFailure({
            given: `${org2} trying to upgrade to ${statusStrings[thrivable]}`,
            should: 'fail',
            actual: await makereptableWithError(org2, { authorization: `${org2}@active` })
        })
    }
    await makeStatus(org2, thrivable)
    
    await contracts.organization.makereptable(org2, { authorization: `${org2}@active` })
    await checkHistoryRecords(statusStrings[reputable], [org1, org2])

    await contracts.organization.makesustnble(org2, { authorization: `${org2}@active` })
    await checkHistoryRecords(statusStrings[sustainable], [org1, org2])

    await contracts.organization.makeregen(org2, { authorization: `${org2}@active` })
    await checkHistoryRecords(statusStrings[regenerative], [org1, org2])

    await contracts.organization.makethrivble(org2, { authorization: `${org2}@active` })
    await checkHistoryRecords(statusStrings[thrivable], [org2])

    await checkOrgStatus([regenerative, thrivable, regular, regular, regular])

    const checkTrxpoints = async (user, expectedValue) => {
        const trxtables = await getTableRows({
            code: history,
            scope: day,
            table: 'dailytrxs',
            json: true,
            limit: 10000
        })
        const orgTrx = trxtables.rows.filter(r => r.from == user)
        assert({
            given: 'org with status',
            should: 'use its multiplier',
            actual: orgTrx[orgTrx.length-1].from_points,
            expected: parseInt(Math.ceil(expectedValue))
        })
    }

    console.log('transfer to org')
    await contracts.accounts.testsetrs(fourthuser, 49, { authorization: `${accounts}@active` })
    await makeStatus(org5, thrivable)

    const multiplier = 0.98989898989899

    await contracts.accounts.testsetrs(org5, 49, { authorization: `${accounts}@active` })
    await contracts.token.transfer(fourthuser, org5, "100.0000 SEEDS", "Initial supply 1", { authorization: `${fourthuser}@active` })
    await sleep(3000)
    await checkTrxpoints(fourthuser, 100 * multiplier * 1.0)

    await makeStatus(org5, reputable)
    await contracts.organization.makereptable(org5, { authorization: `${org5}@active` })
    await contracts.accounts.testsetrs(org5, 49, { authorization: `${accounts}@active` })
    await contracts.token.transfer(fourthuser, org5, "110.0000 SEEDS", "Initial supply 2", { authorization: `${fourthuser}@active` })
    await sleep(3000)
    await checkTrxpoints(fourthuser, 110 * multiplier * 1.0)

    await makeStatus(org5, sustainable)
    await contracts.organization.makesustnble(org5, { authorization: `${org5}@active` })
    await contracts.accounts.testsetrs(org5, 49, { authorization: `${accounts}@active` })
    await contracts.token.transfer(fourthuser, org5, "120.0000 SEEDS", "Initial supply 3", { authorization: `${fourthuser}@active` })
    await sleep(3000)
    await checkTrxpoints(fourthuser, 120 * multiplier * 1.3)

    await makeStatus(org5, regenerative)
    await contracts.organization.makeregen(org5, { authorization: `${org5}@active` })
    await contracts.accounts.testsetrs(org5, 49, { authorization: `${accounts}@active` })
    await contracts.token.transfer(fourthuser, org5, "130.0000 SEEDS", "Initial supply 4", { authorization: `${fourthuser}@active` })
    await sleep(3000)
    await checkTrxpoints(fourthuser, 130 * multiplier * 1.7)

    await makeStatus(org5, thrivable)
    await contracts.organization.makethrivble(org5, { authorization: `${org5}@active` })
    await contracts.accounts.testsetrs(org5, 49, { authorization: `${accounts}@active` })
    await contracts.token.transfer(fourthuser, org5, "140.0000 SEEDS", "Initial supply 5", { authorization: `${fourthuser}@active` })
    await sleep(3000)
    await checkTrxpoints(fourthuser, 140 * multiplier * 2.0)

})

/* describe('transaction points', async assert => {
    const contracts = await initContracts({ settings, history, accounts, organization, token, harvest })
    let firstorg = 'testorg1'
    let secondorg = 'testorg2'

    const day = getBeginningOfDayInSeconds()

    console.log('settings reset')
    await contracts.settings.reset({ authorization: `${settings}@active` })

    console.log('reset organization')
    await contracts.organization.reset({ authorization: `${organization}@active` })

    console.log('reset token stats')
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log('accounts reset')
    await contracts.accounts.reset({ authorization: `${accounts}@active` })
    
    console.log('harvest reset')
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    console.log('history reset')
    await contracts.history.reset(firstuser, { authorization: `${history}@active` })
    await contracts.history.reset(seconduser, { authorization: `${history}@active` })
    await contracts.history.reset(firstorg, { authorization: `${history}@active` })
    await contracts.history.reset(secondorg, { authorization: `${history}@active` })
    await contracts.history.deldailytrx(day, { authorization: `${history}@active` })
  
    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

    console.log('add rep')
    await contracts.accounts.addrep(firstuser, 10000, { authorization: `${accounts}@active` })
    await contracts.accounts.addrep(seconduser, 13000, { authorization: `${accounts}@active` })

    console.log('create balance')
    await contracts.token.transfer(firstuser, organization, "400.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })
    await contracts.token.transfer(seconduser, organization, "200.0000 SEEDS", "Initial supply", { authorization: `${seconduser}@active` })
    
    console.log('create organization')

    await contracts.organization.create(firstuser, firstorg, "Org Number 1", eosDevKey, { authorization: `${firstuser}@active` })
    await contracts.organization.create(firstuser, secondorg, "Org 2", eosDevKey,  { authorization: `${firstuser}@active` })

    console.log('add rep for these users then make some transactions')
    await contracts.accounts.testsetrs(firstuser, 99, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(seconduser, 25, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(firstorg, 33, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(secondorg, 66, { authorization: `${accounts}@active` })

    console.log('make transfers')
    const transfer = async (a, b, amount) => {
        await contracts.token.transfer(a, b, amount, Math.random().toString(36).substring(7), { authorization: `${a}@active` })
        await sleep(2000)
    }
    
    await transfer(firstuser, firstorg, "10.0000 SEEDS")
    await transfer(firstuser, secondorg, "19.0000 SEEDS")
    await transfer(secondorg, firstorg, "19.0000 SEEDS")
    await transfer(firstorg, seconduser, "13.0000 SEEDS")
    await transfer(seconduser, firstorg, "7.0000 SEEDS")
    
    // const orgTx1 = await getTableRows({
    //     code: history,
    //     scope: firstorg,
    //     table: 'orgtx',
    //     json: true
    // })

    // const orgTx2 = await getTableRows({
    //     code: history,
    //     scope: secondorg,
    //     table: 'orgtx',
    //     json: true
    // })

    // const userTx1 = await getTableRows({
    //     code: history,
    //     scope: firstuser,
    //     table: 'transactions',
    //     json: true
    // })
    // const userTx2 = await getTableRows({
    //     code: history,
    //     scope: seconduser,
    //     table: 'transactions',
    //     json: true
    // })

    // console.log("org1 tx "+JSON.stringify(orgTx1, null, 2))
    // console.log("org2 tx "+JSON.stringify(orgTx2, null, 2))
    // console.log("user1 tx "+JSON.stringify(userTx1, null, 2))
    // console.log("user2 tx "+JSON.stringify(userTx2, null, 2))

    // let t1 = await contracts.history.orgtxpt(firstorg, 0, 200, 0, { authorization: `${history}@active` })
    // printConsole(t1)

    await contracts.harvest.calctrxpts({ authorization: `${harvest}@active` })

    // let txt = await contracts.organization.scoreorgs(firstorg, { authorization: `${organization}@active` })

    let checkTxPoints = async () => {
        const txpoints = await getTableRows({
            code: harvest,
            scope: 'org',
            table: 'txpoints',
            json: true
        })    
        
        console.log("harvest txpoints "+JSON.stringify(txpoints, null, 2))
    
    }

    const trxPoints = await getTableRows({
        code: history,
        scope: firstorg,
        table: 'trxpoints',
        json: true
    })
    console.log(trxPoints)

    await checkTxPoints()

    // await contracts.organization.scoreorgs(secondorg, { authorization: `${organization}@active` })

    // await checkTxPoints()

})

const printConsole = (objResult) => {
    const str = ""+JSON.stringify(objResult, null, 2)
    const lines = str.split("\n")
    for (var i=0; i<lines.length; i++) {
        const l = lines[i].trim()
        if (l.startsWith('"console')) {
            console.log(l)
        }
    }
}
 */