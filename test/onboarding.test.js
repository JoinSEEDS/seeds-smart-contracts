const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, sha256, fromHexString, isLocal, ramdom64ByteHexString, createKeypair, getBalance, sleep } = require('../scripts/helper')
const { filter, all } = require('ramda')

const { onboarding, token, accounts, harvest, firstuser, seconduser, thirduser, fourthuser, region, settings } = names

const randomAccountName = () => {
    let length = 12
    var result = '';
    var characters = 'abcdefghijklmnopqrstuvwxyz1234';
    var charactersLength = characters.length;
    for (var i = 0; i < length; i++) {
        result += characters.charAt(Math.floor(Math.random() * charactersLength));
    }
    return result;
}

const randomAccountNamergn = () => {
    let length = 8
    var result = '';
    var characters = 'abcdefghijklmnopqrstuvwxyz1234';
    var charactersLength = characters.length;
    for (var i = 0; i < length; i++) {
        result += characters.charAt(Math.floor(Math.random() * charactersLength));
    }
    return result + ".rgn";
}

const getNumInvites = async () => {
    invites = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'invites',
        json: true,
    })
    return invites.rows.length
}

describe('Onboarding', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest, settings })

    console.log(`reset ${settings}`)
    await contracts.settings.reset({ authorization: `${settings}@active` })

    const transferQuantity = `10.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '15.0000 SEEDS'

    const newAccount = randomAccountName()
    const newAccount2 = randomAccountName()
    console.log("New account " + newAccount)

    const keyPair = await createKeypair()
    const keyPair2 = await createKeypair()

    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))

    const newAccountPublicKey = keyPair.public
    const newAccountPublicKey2 = keyPair2.public

    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const inviteSecret2 = await ramdom64ByteHexString()
    const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

    const inviteSecret3 = await ramdom64ByteHexString()
    const inviteHash3 = sha256(fromHexString(inviteSecret3)).toString('hex')

    const inviteSecret4 = await ramdom64ByteHexString()
    const inviteHash4 = sha256(fromHexString(inviteSecret4)).toString('hex')

    const reset = async () => {
        if (!isLocal()) {
            console.log("Don't reset contracts on mainnet or testnet")
            return
        }

        console.log(`reset ${accounts}`)
        await contracts.accounts.reset({ authorization: `${accounts}@active` })

        console.log(`reset ${token}`)
        await contracts.token.resetweekly({ authorization: `${token}@active` })

        console.log(`reset ${onboarding}`)
        await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

        console.log(`reset ${harvest}`)
        await contracts.harvest.reset({ authorization: `${harvest}@active` })
    }

    const deposit = async (user, memo = '') => {
        console.log(`${token}.transfer from ${user} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(user, onboarding, totalQuantity, memo, { authorization: `${user}@active` })
    }

    const invite = async (user, inviteHash) => {
        console.log(`${onboarding}.invite from ${user}`)
        await contracts.onboarding.invite(user, transferQuantity, sowQuantity, inviteHash, { authorization: `${user}@active` })
    }

    const accept = async () => {
        console.log(`onboarding accept - ${onboarding}.accept from ${newAccount}`)
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })
    }

    await reset()

    console.log(`adding users ${accounts}.adduser ${firstuser}`)
    await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })
    await contracts.accounts.adduser(fourthuser, '', 'individual', { authorization: `${accounts}@active` })

    await deposit(firstuser)

    await invite(firstuser, inviteHash)

    await deposit(seconduser)

    await invite(seconduser, inviteHash2)

    await accept()

    const allInvites = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'invites',
        json: true
    })

    const invitesBySponsor = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'invites',
        key_type: 'name',
        index_position: 3,
        lower_bound: seconduser,
        upper_bound: seconduser,
        json: true,
    })

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const getSponsors = async () => {
        return await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
    }

    console.log("first user makes deposit on behalf of third user")
    await contracts.token.transfer(firstuser, onboarding, '22.0000 SEEDS', "sponsor " + thirduser, { authorization: `${firstuser}@active` })

    let sponsorsAfter = await getSponsors()

    console.log("first user invites on behalf of fourth user")
    await contracts.token.transfer(firstuser, onboarding, '16.0000 SEEDS', "", { authorization: `${firstuser}@active` })
    await contracts.onboarding.invitefor(firstuser, fourthuser, "11.0000 SEEDS", "5.0000 SEEDS", inviteHash3, { authorization: `${firstuser}@active` })

    console.log(`${onboarding}.accept from ${newAccount2}`)
    await contracts.onboarding.accept(newAccount2, inviteSecret3, newAccountPublicKey2, { authorization: `${onboarding}@active` })

    let invitesAfter = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'invites',
        key_type: 'name',
        index_position: 3,
        lower_bound: fourthuser,
        upper_bound: fourthuser,
        json: true,
    })

    let referrersA = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'referrers',
        json: true,
    })

    const refs = await getTableRows({
        code: accounts,
        scope: accounts,
        table: 'refs',
        json: true
    })

    //console.log("invitefor - after "+ JSON.stringify(invitesAfter, null, 2))

    //console.log("REFS - after "+ JSON.stringify(refs, null, 2))

    //console.log("invitefor - after referrersA "+ JSON.stringify(referrersA, null, 2))

    let refererOfNewAccount = refs.rows.filter((item) => item.invited == newAccount2)

    const newUserHarvest = rows.find(row => row.account === newAccount)

    console.log("cancel invite")
    let getNumReferrers = async () => {
        referrers = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'referrers',
            json: true,
        })
        return referrers.rows.length
    }

    console.log("testing cancel")
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })
    await contracts.token.transfer(firstuser, onboarding, '16.0000 SEEDS', "", { authorization: `${firstuser}@active` })
    let sponsors2 = await getSponsors()

    //console.log("sponsors2 "+JSON.stringify(sponsors2, null, 2))

    await contracts.onboarding.invite(firstuser, "11.0000 SEEDS", "5.0000 SEEDS", inviteHash2, { authorization: `${firstuser}@active` })
    let invites1 = await getNumInvites()
    let referrers1 = await getNumReferrers()

    console.log("cancel from account other than sponsor")
    let otherCancel = false
    try {
        await contracts.onboarding.cancel(seconduser, inviteHash2, { authorization: `${seconduser}@active` })
        otherCancel = true
    } catch (err) {
        if (("" + err).indexOf("not sponsor") == -1) {
            console.log("unexpected error cancel " + err)
        }
    }


    console.log("cancel")
    let b1 = await getBalance(firstuser)
    await contracts.onboarding.cancel(firstuser, inviteHash2, { authorization: `${firstuser}@active` })
    let b1_after = await getBalance(firstuser)

    let invites1_after = await getNumInvites()
    let referrers1_after = await getNumReferrers()

    console.log("testing cancel after invitefor")
    await contracts.token.transfer(firstuser, onboarding, '17.0000 SEEDS', "", { authorization: `${firstuser}@active` })
    await contracts.onboarding.invitefor(firstuser, fourthuser, "12.0000 SEEDS", "5.0000 SEEDS", inviteHash2, { authorization: `${firstuser}@active` })

    let invites2 = await getNumInvites()
    let referrers2 = await getNumReferrers()

    console.log("cancel after invitefor")
    await contracts.onboarding.cancel(firstuser, inviteHash2, { authorization: `${firstuser}@active` })
    let invites2_after = await getNumInvites()
    let referrers2_after = await getNumReferrers()

    assert({
        given: 'Cancel from non sponsor account',
        should: 'only sponsor can cancel',
        actual: otherCancel,
        expected: false
    })

    assert({
        given: 'invite cancel',
        should: 'end up with no invites',
        actual: [invites1, referrers1, invites1_after, referrers1_after],
        expected: [1, 0, 0, 0]
    })

    assert({
        given: 'invite cancel',
        should: 'return balance',
        actual: b1_after,
        expected: b1 + 16
    })


    assert({
        given: 'invite with referrer cancel',
        should: 'end up with no invites',
        actual: [invites2, referrers2, invites2_after, referrers2_after],
        expected: [1, 1, 0, 0]
    })

    assert({
        given: 'invited new user',
        should: 'have planted seeds',
        actual: newUserHarvest,
        expected: {
            account: newAccount,
            planted: sowQuantity,
            reward: '0.0000 SEEDS'
        }
    })

    assert({
        given: 'created two invites',
        should: 'have two invites',
        actual: allInvites.rows.length,
        expected: 2
    })

    assert({
        given: 'search by sponsor user 2',
        should: 'have 1 result',
        actual: invitesBySponsor.rows.length,
        expected: 1
    })

    assert({
        given: 'add invite for referrer',
        should: 'have 1 result',
        actual: referrersA.rows.length,
        expected: 1
    })

    assert({
        given: 'search by sponsor user 2',
        should: 'contains sponsor',
        actual: invitesBySponsor.rows[0].sponsor,
        expected: seconduser
    })


    //console.log("refererOfNewAccount "+JSON.stringify(refererOfNewAccount, null, 2))

    assert({
        given: 'search by referrer user 4',
        should: 'have 1 result',
        actual: refererOfNewAccount.length,
        expected: 1
    })

    assert({
        given: 'search by referrer user 4',
        should: 'contains referrer',
        actual: refererOfNewAccount[0].referrer,
        expected: fourthuser
    })


    assert({
        given: 'define sponsor in memo',
        should: 'have sponsors table entry',
        actual: sponsorsAfter.rows[2],
        expected: {
            "account": thirduser,
            "balance": "22.0000 SEEDS"
        }
    })

})

describe('Use application permission to accept', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `0.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '5.0000 SEEDS'

    const newAccount = randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`adduser (${firstuser})`)
    await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })

    await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(firstuser, 22, { authorization: `${accounts}@active` })

    console.log(`transfer from ${firstuser} to ${onboarding} (${totalQuantity})`)
    await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })

    console.log(`invite from ${firstuser}`)
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })

    const vouchBeforeInvite = await eos.getTableRows({
        code: accounts,
        scope: newAccount,
        table: 'vouch',
        json: true
    })

    console.log(`accept from Application`)
    await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })

    const acceptUsers = await eos.getTableRows({
        code: accounts,
        scope: accounts,
        table: 'users',
        json: true
    })
    //console.log("users "+JSON.stringify(acceptUsers, null, 2))

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const vouchAfterInvite = await eos.getTableRows({
        code: accounts,
        scope: accounts,
        table: 'vouches',
        json: true
    })

    //console.log("vouch after accept "+JSON.stringify(vouchAfterInvite, null, 2))

    const newUserHarvest = rows.find(row => row.account === newAccount)

    assert({
        given: 'invited new user',
        should: 'have planted seeds',
        actual: newUserHarvest,
        expected: {
            account: newAccount,
            planted: sowQuantity,
            reward: '0.0000 SEEDS'
        }
    })
    assert({
        given: 'invited new user',
        should: 'have new vouch entry',
        actual: [vouchBeforeInvite.rows.length, vouchAfterInvite.rows.length],
        expected: [0, 1]
    })

})

describe('Invite from non-seeds user - sp', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `0.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '5.0000 SEEDS'

    const newAccount = randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const reset = async () => {
        if (!isLocal()) {
            console.log("Don't reset contracts on mainnet or testnet")
            return
        }

        console.log(`reset ${accounts}`)
        await contracts.accounts.reset({ authorization: `${accounts}@active` })

        console.log(`reset ${onboarding}`)
        await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

        console.log(`reset ${harvest}`)
        await contracts.harvest.reset({ authorization: `${harvest}@active` })
    }

    const adduser = async () => {
        try {
            console.log(`${accounts}.adduser (${firstuser})`)
            await contracts.accounts.adduser(firstuser, "", "individual", { authorization: `${accounts}@active` })
        } catch (error) {
            console.log("user exists")
        }
    }

    const deposit = async (user) => {
        console.log(`${token}.transfer from ${user} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(user, onboarding, totalQuantity, '', { authorization: `${user}@active` })
    }

    const invite = async () => {
        console.log(`${onboarding}.invite from ${firstuser}`)
        await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })
    }

    await reset()

    await deposit(firstuser)

    await invite()

    console.log(`onboarding accept from Application for ` + newAccount)
    await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const newUserHarvest = rows.find(row => row.account === newAccount)

    assert({
        given: 'invited new user',
        should: 'have planted seeds',
        actual: newUserHarvest,
        expected: {
            account: newAccount,
            planted: sowQuantity,
            reward: '0.0000 SEEDS'
        }
    })
})

describe('Campaign reward for existing user', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `22.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '27.0000 SEEDS'

    const newAccount = seconduser;// randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const inviteSecret2 = await ramdom64ByteHexString()
    const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

    const inviteSecret3 = await ramdom64ByteHexString()
    const inviteHash3 = sha256(fromHexString(inviteSecret3)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${token}`)
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`reset ${harvest}`)
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    const adduser = async (user) => {
        try {
            console.log(`${accounts}.adduser (${user})`)
            await contracts.accounts.adduser(user, '', 'individual', { authorization: `${accounts}@active` })
        } catch (error) {
            console.log("user exists")
        }
    }

    const deposit = async (user) => {
        console.log(`${token}.transfer from ${user} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(user, onboarding, totalQuantity, '', { authorization: `${user}@active` })
    }

    const invite = async () => {
        console.log(`${onboarding}.invite from ${firstuser}`)
        await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })
    }

    const reward = async () => {
        console.log(`Existing user reward from Application`)
        await contracts.onboarding.reward(newAccount, inviteSecret3, { authorization: `${newAccount}@active` })
    }

    await adduser(firstuser)

    const before = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const beforeBalance = await getBalance(seconduser)

    await deposit(firstuser)

    await invite()

    console.log(`Existing user accept from Application`)
    await contracts.onboarding.acceptexist(newAccount, inviteSecret, { authorization: `${newAccount}@active` })

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    let invites3_before = await getNumInvites()

    console.log("cleanup")
    await contracts.onboarding.cleanup(0, 100, 100, { authorization: `${onboarding}@active` })

    let invites3_after = await getNumInvites()

    console.log(`${onboarding}.invite from ${firstuser}`)
    await deposit(firstuser)
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash2, { authorization: `${firstuser}@active` })

    console.log(`invite from for 7 from ${firstuser}`)
    await contracts.token.transfer(firstuser, onboarding, "7.0000 SEEDS", '', { authorization: `${firstuser}@active` })
    await contracts.onboarding.invite(firstuser, "2.0000 SEEDS", "5.0000 SEEDS", inviteHash3, { authorization: `${firstuser}@active` })

    const beforeRewardBalance = await getBalance(newAccount)
    await reward()
    const afterRewardBalance = await getBalance(newAccount)

    const balances3 = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    console.log("cleanup 2")
    await contracts.onboarding.cleanup(0, 100, 100, { authorization: `${onboarding}@active` })
    let invites4_after = await getNumInvites()

    const newUserHarvest = rows.find(row => row.account === newAccount)
    const newUserHarvest3 = balances3.rows.find(row => row.account === newAccount)

    assert({
        given: 'invited new user',
        should: 'have planted seeds',
        actual: newUserHarvest,
        expected: {
            account: newAccount,
            planted: "5.0000 SEEDS",
            reward: '0.0000 SEEDS'
        }
    })

    assert({
        given: 'accept reward planted',
        should: 'have planted seeds (no change)',
        actual: newUserHarvest3,
        expected: {
            account: newAccount,
            planted: "5.0000 SEEDS",
            reward: '0.0000 SEEDS'
        }
    })

    assert({
        given: 'accept reward cash',
        should: 'have 2 seeds more',
        actual: afterRewardBalance - beforeRewardBalance,
        expected: 7
    })

    assert({
        given: 'called cleanup',
        should: 'same number of invites',
        actual: invites3_after,
        expected: invites3_before
    })
    assert({
        given: 'called cleanup',
        should: 'one less number of invites',
        actual: invites4_after,
        expected: 2
    })
})


describe('Create region', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, region })

    const newAccount = randomAccountNamergn()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public

    await contracts.onboarding.createregion(firstuser, newAccount, newAccountPublicKey, { authorization: `${onboarding}@active` })

    var hasNewDomain = false
    try {
        const accountInfo = await eos.getAccount(newAccount)
        hasNewDomain = true;
    } catch {

    }

    assert({
        given: 'created region',
        should: 'have account',
        actual: hasNewDomain,
        expected: true
    })
})


describe('Private campaign', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest, settings })

    console.log(`reset ${settings}`)
    await contracts.settings.reset({ authorization: `${settings}@active` })

    const newAccountPublicKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const inviteSecret2 = await ramdom64ByteHexString()
    const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

    const inviteSecret3 = await ramdom64ByteHexString()
    const inviteHash3 = sha256(fromHexString(inviteSecret3)).toString('hex')

    const inviteSecret4 = await ramdom64ByteHexString()
    const inviteHash4 = sha256(fromHexString(inviteSecret4)).toString('hex')

    const inviteSecret5 = await ramdom64ByteHexString()
    const inviteHash5 = sha256(fromHexString(inviteSecret5)).toString('hex')

    const checkCampaignFunds = async (campaignId, expectedFunds) => {
        const camps = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'campaigns',
            json: true
        })
        const funds = (camps.rows.filter(r => r.campaign_id === campaignId)[0]).remaining_amount
        assert({
            given: `invite created`,
            should: `have the correct remaining funds`,
            actual: funds,
            expected: expectedFunds
        })
    }

    const checkCampaignInvites = async (campaignId, expectedLength, newInvite) => {
        const campinvites = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'campinvites',
            json: true
        })
        const invs = campinvites.rows.filter(r => r.campaign_id === campaignId).map(r => r.invite_id)
        assert({
            given: `invite created`,
            should: `have the correct campinvites entries`,
            actual: [invs.length, invs.includes(newInvite)],
            expected: [expectedLength, true]
        })
    }

    const checkUsers = async (newUser, numberOfUsers) => {
        const users = await getTableRows({
            code: accounts,
            scope: accounts,
            table: 'users',
            json: true
        })
        const expectedUser = users.rows.filter(r => r.account === newUser)[0]
        assert({
            given: `given ${newUser} accepted invitation`,
            should: 'have the correct number of users',
            actual: [users.rows.length, expectedUser.account],
            expected: [numberOfUsers, newUser]
        })
    }

    const checkVouches = async (user, number) => {
        const vouches = await getTableRows({
            code: accounts,
            scope: accounts,
            table: 'vouches',
            json: true
        })
        assert({
            given: `${user} vouched`,
            should: 'have the correct number of vouchers',
            actual: (vouches.rows.filter(r => r.account === user)).length,
            expected: number
        })
    }

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${token}`)
    await contracts.token.resetweekly({ authorization: `${token}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log('join users')
    await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
    await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(firstuser, 50, { authorization: `${accounts}@active` })

    const deposit = async (from, to, quantity, memo = '') => {
        console.log(`${token}.transfer from ${from} to ${to} (${quantity})`)
        await contracts.token.transfer(from, to, quantity, memo, { authorization: `${from}@active` })
    }

    console.log('create campaign')
    const maxAmount1 = '20.0000 SEEDS'
    const maxAmount2 = '30.0000 SEEDS'
    await deposit(firstuser, onboarding, '5.0000 SEEDS')

    let cantCreateCampWithNotEnoughFunds = true
    try {
        await contracts.onboarding.createcampg(firstuser, firstuser, '10.0000 SEEDS', '5.0000 SEEDS', firstuser, '1.0000 SEEDS', maxAmount1, 0, { authorization: `${firstuser}@active` })
        cantCreateCampWithNotEnoughFunds = false
    } catch (err) {
        console.log('not enough funds (expected)')
    }

    await deposit(firstuser, onboarding, '45.0000 SEEDS')
    await contracts.onboarding.createcampg(firstuser, firstuser, '10.0000 SEEDS', '6.0000 SEEDS', firstuser, '1.0000 SEEDS', maxAmount1, 0, { authorization: `${firstuser}@active` })
    await contracts.onboarding.createcampg(firstuser, firstuser, '15.0000 SEEDS', '5.0000 SEEDS', firstuser, '2.0000 SEEDS', maxAmount2, 0, { authorization: `${firstuser}@active` })

    await checkCampaignFunds(1, maxAmount1)
    await checkCampaignFunds(2, maxAmount2)

    let user2 = randomAccountName()
    console.log(`invite new user ${user2}`)
    const firstuserBalanceBeforeAccept = await getBalance(firstuser)
    await contracts.onboarding.campinvite(1, firstuser, '6.0000 SEEDS', '3.0000 SEEDS', inviteHash, { authorization: `${firstuser}@active` })

    await contracts.onboarding.accept(user2, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })

    await checkCampaignFunds(1, '10.0000 SEEDS')
    await checkCampaignInvites(1, 1, 0)
    await checkUsers(user2, 2)
    await checkVouches(user2, 1)

    console.log(`addauthorized ${user2}`)
    await contracts.onboarding.addauthorized(1, user2, { authorization: `${firstuser}@active` })

    console.log(`testing max invite amount`)
    let cantExceedMaxInviteAmount = true
    try {
        await contracts.onboarding.campinvite(1, user2, '6.0000 SEEDS', '9.0000 SEEDS', inviteHash2, { authorization: `${user2}@active` })
        cantExceedMaxInviteAmount = false
    } catch (err) {
        console.log('can not exceed max invitation amount (expected)')
    }

    console.log(`${user2} invites ${thirduser}`)
    await contracts.onboarding.campinvite(1, user2, '6.0000 SEEDS', '1.0000 SEEDS', inviteHash2, { authorization: `${user2}@active` })
    await contracts.onboarding.acceptexist(thirduser, inviteSecret2, { authorization: `${thirduser}@active` })

    const firstuserBalanceAfterAccept = await getBalance(firstuser)

    await checkCampaignFunds(1, '2.0000 SEEDS')
    await checkCampaignInvites(1, 2, 1)
    await checkUsers(thirduser, 3)
    await checkVouches(thirduser, 1)

    console.log(`remove ${user2} from the authorization list`)
    await contracts.onboarding.remauthorized(1, user2, { authorization: `${firstuser}@active` })

    console.log('invite without permission')
    let cantInviteWithoutPermission = true
    try {
        await contracts.onboarding.campinvite(1, user2, '6.0000 SEEDS', '1.0000 SEEDS', inviteHash3, { authorization: `${user2}@active` })
        cantInviteWithoutPermission = false
    } catch (err) {
        console.log('can not invite without permission (expected)')
    }

    await contracts.onboarding.addauthorized(1, thirduser, { authorization: `${firstuser}@active` })

    let cantExceedRemainingAmount = true
    try {
        await contracts.onboarding.campinvite(1, thirduser, '6.0000 SEEDS', '1.0000 SEEDS', inviteHash3, { authorization: `${thirduser}@active` })
        cantExceedRemainingAmount = false
    } catch (err) {
        console.log('can not exceed the remaining amount (expected)')
    }

    console.log('retire funds')
    const firstuserBalanceBefore = await getBalance(firstuser)
    await contracts.onboarding.returnfunds(1, { authorization: `${firstuser}@active` })
    const firstuserBalanceAfter = await getBalance(firstuser)

    console.log('cancel invite')
    await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

    const firstuserBalanceBeforeCancel2 = await getBalance(firstuser)

    await contracts.onboarding.campinvite(2, firstuser, '5.0000 SEEDS', '1.0000 SEEDS', inviteHash3, { authorization: `${firstuser}@active` })
    await contracts.onboarding.campinvite(2, firstuser, '5.0000 SEEDS', '1.0000 SEEDS', inviteHash4, { authorization: `${firstuser}@active` })
    await checkCampaignFunds(2, '14.0000 SEEDS')

    await contracts.onboarding.addauthorized(2, thirduser, { authorization: `${firstuser}@active` })
    await contracts.onboarding.cancel(thirduser, inviteHash4, { authorization: `${thirduser}@active` })
    await contracts.onboarding.campinvite(2, thirduser, '5.0000 SEEDS', '1.0000 SEEDS', inviteHash4, { authorization: `${thirduser}@active` })
    await contracts.onboarding.campinvite(2, thirduser, '5.0000 SEEDS', '5.0000 SEEDS', inviteHash5, { authorization: `${thirduser}@active` })
    await checkCampaignFunds(2, '2.0000 SEEDS')

    await contracts.onboarding.returnfunds(2, { authorization: `${firstuser}@active` })

    await sleep(5000)

    const firstuserBalanceAfterCancel2 = await getBalance(firstuser)

    const camps = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'campaigns',
        json: true
    })

    const campinvites = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'campinvites',
        json: true
    })

    const invites = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'invites',
        json: true
    })

    assert({
        given: 'origin with not enough funds',
        should: 'fail',
        actual: cantCreateCampWithNotEnoughFunds,
        expected: true
    })

    assert({
        given: 'user invites with no permission',
        should: 'fail',
        actual: cantInviteWithoutPermission,
        expected: true
    })

    assert({
        given: 'user invites using more than the max amount allowed',
        should: 'fail',
        actual: cantExceedMaxInviteAmount,
        expected: true
    })

    assert({
        given: 'user invites with not enough remaining amount',
        should: 'fail',
        actual: cantExceedRemainingAmount,
        expected: true
    })

    assert({
        given: `${firstuser} call returnfunds`,
        should: 'return the correct amount',
        actual: firstuserBalanceAfter - firstuserBalanceBefore,
        expected: 2
    })

    assert({
        given: 'invites been accepted',
        should: `${firstuser} has more funds`,
        actual: firstuserBalanceAfterAccept - firstuserBalanceBeforeAccept,
        expected: 2
    })

    assert({
        given: `${firstuser} call returnfunds for camapaign 2`,
        should: 'cancel the campaign, the invites and return all the funds',
        actual: firstuserBalanceAfterCancel2 - firstuserBalanceBeforeCancel2,
        expected: 30
    })

    assert({
        given: 'all the campaigns finished',
        should: 'have the correct length',
        actual: [camps.rows.length, campinvites.rows.length, invites.rows.length],
        expected: [0, 0, 2]
    })

})


// describe("clean up unused invites", async assert => {

//     if (!isLocal()) {
//         console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
//         return
//     }

//     const transferQuantity = `10.0000 SEEDS`
//     const sowQuantity = '5.0000 SEEDS'
//     const totalQuantity = '15.0000 SEEDS'

//     const newAccount = randomAccountName()
//     const newAccount2 = randomAccountName()

//     const keyPair = await createKeypair()
//     const keyPair2 = await createKeypair()

//     console.log("new account keys: " + JSON.stringify(keyPair, null, 2))

//     const contracts = await initContracts({ onboarding, token, accounts, harvest, settings })

//     console.log(`reset ${settings}`)
//     await contracts.settings.reset({ authorization: `${settings}@active` })

//     console.log(`reset ${accounts}`)
//     await contracts.accounts.reset({ authorization: `${accounts}@active` })

//     console.log(`reset ${token}`)
//     await contracts.token.resetweekly({ authorization: `${token}@active` })

//     console.log(`reset ${onboarding}`)
//     await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

//     console.log('join users')
//     await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
//     await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })

//     const newAccountPublicKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

//     const inviteSecret = await ramdom64ByteHexString()
//     const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

//     const inviteSecret2 = await ramdom64ByteHexString()
//     const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

//     const inviteSecret3 = await ramdom64ByteHexString()
//     const inviteHash3 = sha256(fromHexString(inviteSecret3)).toString('hex')

//     const deposit = async (user, memo = '') => {
//         console.log(`${token}.transfer from ${user} to ${onboarding} (${totalQuantity})`)
//         await contracts.token.transfer(user, onboarding, totalQuantity, memo, { authorization: `${user}@active` })
//     }

//     const invite = async (user, inviteHash) => {
//         console.log(`${onboarding}.invite from ${user}`)
//         await contracts.onboarding.invite(user, transferQuantity, sowQuantity, inviteHash, { authorization: `${user}@active` })
//     }

//     const checkInvites = async (numberInvites, expectedTimestamps) => {
//         const allInvites = await getTableRows({
//             code: onboarding,
//             scope: onboarding,
//             table: 'invites',
//             json: true
//         })
//         console.log(allInvites)

//         assert({
//             given: 'cleanup ran',
//             should: 'have the correct number of invites',
//             actual: allInvites.rows.length,
//             expected: numberInvites
//         })

//         const timestamps = await getTableRows({
//             code: onboarding,
//             scope: onboarding,
//             table: 'timestamps',
//             json: true
//         })
//         console.log(timestamps)

//         assert({
//             given: 'cleanup ran',
//             should: 'have the correct timestamp entries',
//             actual: timestamps.rows.map(r => {
//                 delete r.timestamp
//                 return { ...r }
//             }),
//             expected: expectedTimestamps
//         })
//     }

//     await deposit(firstuser)
//     await invite(firstuser, inviteHash)
//     await deposit(seconduser)
//     await invite(seconduser, inviteHash2)
//     await deposit(seconduser)
//     await invite(seconduser, inviteHash3)

//     await contracts.onboarding.chkcleanup({ authorization: `${onboarding}@active` })
//     await sleep(500)

//     await checkInvites(3, [{ id: 0, invite_id: 2 }])

//     await sleep(2000)

//     await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

//     await contracts.onboarding.chkcleanup({ authorization: `${onboarding}@active` })
//     await sleep(4000)

//     await checkInvites(1, [
//         { id: 0, invite_id: 2 },
//         { id: 1, invite_id: 2 }
//     ])

// })


describe('Invite amounts', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `0.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '5.0000 SEEDS'

    const newAccount = randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`reset ${harvest}`)
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    await contracts.accounts.adduser(firstuser, "", "individual", { authorization: `${accounts}@active` })

    await contracts.token.transfer(firstuser, onboarding, '4.9999 SEEDS', '', { authorization: `${firstuser}@active` })

    var fail1 = false
    try {
        await contracts.onboarding.invite(firstuser, transferQuantity, '3.0000 SEEDS', inviteHash, { authorization: `${firstuser}@active` })
    } catch (err) {
        fail1 = true
        // expected
    }

    var fail2 = false
    try {
        await contracts.onboarding.invite(firstuser, transferQuantity, '5.0000 SEEDS', inviteHash, { authorization: `${firstuser}@active` })
    } catch (err) {
        fail2 = true
        // expected
    }

    console.log("increase balance")
    await contracts.token.transfer(firstuser, onboarding, '0.0001 SEEDS', '', { authorization: `${firstuser}@active` })
    
    console.log("invite")
    await contracts.onboarding.invite(firstuser, transferQuantity, '5.0000 SEEDS', inviteHash, { authorization: `${firstuser}@active` })

    succeed = true // if we get here...

    assert({
        given: 'too low sow',
        should: 'fail to invite',
        actual: fail1,
        expected: true
    })
    assert({
        given: 'too low balance',
        should: 'fail to invite',
        actual: fail2,
        expected: true
    })
    assert({
        given: 'enough sow',
        should: 'succeed to invite',
        actual: succeed,
        expected: true
    })
})

describe('Ban', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `0.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '5.0000 SEEDS'

    const newAccount = randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`reset ${harvest}`)
    await contracts.harvest.reset({ authorization: `${harvest}@active` })

    await contracts.accounts.adduser(firstuser, "", "individual", { authorization: `${accounts}@active` })
    await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })
    
    console.log("invite")
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })

    await contracts.accounts.bantree(firstuser, false, { authorization: `${accounts}@active` })

    console.log("accept new account from ban "+newAccount)

    isBan = false
    try {
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })
    } catch (err) {
        message = ""+err
        isBan = message.indexOf("banned user") != -1
    }

    await contracts.accounts.unban(firstuser, { authorization: `${accounts}@active` })

    await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })

    accepted = true // if we get here...

    assert({
        given: 'ban',
        should: 'cant accept invite',
        actual: isBan,
        expected: true
    })

    assert({
        given: 'unban',
        should: 'can accept invite',
        actual: accepted,
        expected: true
    })
})

describe('Cancel invites', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `0.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '5.0000 SEEDS'

    const newAccount = randomAccountName()
    const newAccount2 = randomAccountName()
    console.log("New account " + newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public

    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const inviteSecret2 = await ramdom64ByteHexString()
    const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })
    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })
    console.log(`reset ${harvest}`)
    await contracts.harvest.reset({ authorization: `${harvest}@active` })
    console.log("add user")
    await contracts.accounts.adduser(firstuser, "", "individual", { authorization: `${accounts}@active` })
    
    //// 
    //// Cancel then accept
    ////
    console.log("Cancel then accept")

    var balanceBefore1 = await getBalance(firstuser)
    console.log("balance: "+balanceBefore1)

    await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })
    
    var balance2 = await getBalance(firstuser)
    console.log("balance2: "+balance2)

    console.log("invite")
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })

    console.log("cancel")
    await contracts.onboarding.cancel(firstuser, inviteHash, { authorization: `${firstuser}@active` })

    console.log("accept new account "+newAccount)
    isAccept = true
    try {
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })
    } catch (err) {
        console.log("expected error: "+err)
        isAccept = false
    }
    var balanceAfterCancel1 = await getBalance(firstuser)
    console.log("balance2: "+balanceAfterCancel1)

    //// 
    //// Accept then cancel
    ////

    console.log("Accept then cancel")

    var balanceBefore2 = await getBalance(firstuser)
    console.log("balanceBefore2: "+balanceBefore2)

    await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })
    
    var balance22 = await getBalance(firstuser)
    console.log("balance22: "+balance22)

    console.log("invite 2")
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash2, { authorization: `${firstuser}@active` })

    console.log("accept 2 - should work")
    await contracts.onboarding.accept(newAccount2, inviteSecret2, newAccountPublicKey, { authorization: `${onboarding}@active` })

    isCancel = true
    try {
        console.log("cancel 2")
        await contracts.onboarding.cancel(firstuser, inviteHash2, { authorization: `${firstuser}@active` })
        } catch (err) {
        console.log("expected error: "+err)
        isCancel = false
    }
    var balanceAfterCancel2 = await getBalance(firstuser)
    console.log("balanceAfterCancel2: "+balanceAfterCancel2)

    assert({
        given: 'invite was cancelled',
        should: 'cannot accept invite',
        actual: isAccept,
        expected: false
    })

    assert({
        given: 'invite was cancelled',
        should: 'return money',
        actual: balanceBefore1,
        expected: balanceAfterCancel1
    })

    assert({
        given: 'invite was accepted',
        should: 'cannot cancel invite',
        actual: isCancel,
        expected: false
    })

    assert({
        given: 'invite was accepted',
        should: 'do not return money',
        actual: balanceAfterCancel2,
        expected: balanceBefore2 - 5
    })

})
