const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, sha256, isLocal, ramdom64ByteHexString, createKeypair, getBalance } = require('../scripts/helper')

const { onboarding, token, accounts, harvest, firstuser, seconduser, thirduser, fourthuser, bioregion } = names

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
 
 const randomAccountNameBDC = () => {
    let length = 8
    var result           = '';
    var characters       = 'abcdefghijklmnopqrstuvwxyz1234';
    var charactersLength = characters.length;
    for ( var i = 0; i < length; i++ ) {
       result += characters.charAt(Math.floor(Math.random() * charactersLength));
    }
    return result + ".bdc";
  }
  
const fromHexString = hexString =>
  new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))

describe('Onboarding', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }
    
    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `10.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '15.0000 SEEDS'
    
    const newAccount = randomAccountName()
    const newAccount2 = randomAccountName()
    console.log("New account "+newAccount)

    const keyPair = await createKeypair()
    const keyPair2 = await createKeypair()

    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))

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
    await contracts.token.transfer(firstuser, onboarding, '22.0000 SEEDS', "sponsor "+thirduser, { authorization: `${firstuser}@active` })    

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
    
    console.log("invitefor - after referrersA "+ JSON.stringify(referrersA, null, 2))

    let refererOfNewAccount = refs.rows.filter( (item) => item.invited == newAccount2)

    const newUserHarvest = rows.find(row => row.account === newAccount)

    console.log("cancel invite")
    let getNumInvites = async () => {
        invites = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'invites',
            json: true,
        })
        return invites.rows.length

    }
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

    console.log("sponsors2 "+JSON.stringify(sponsors2, null, 2))

    await contracts.onboarding.invite(firstuser, "11.0000 SEEDS", "5.0000 SEEDS", inviteHash2, { authorization: `${firstuser}@active` })
    let invites1 = await getNumInvites()
    let referrers1 = await getNumReferrers()

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


    console.log("refererOfNewAccount "+JSON.stringify(refererOfNewAccount, null, 2))

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
    console.log("New account "+newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`${accounts}.adduser (${firstuser})`)
    await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })     
    
    await contracts.accounts.testresident(firstuser, { authorization: `${accounts}@active` })
    await contracts.accounts.testsetrs(firstuser, 22, { authorization: `${accounts}@active` })

    console.log(`${token}.transfer from ${firstuser} to ${onboarding} (${totalQuantity})`)
    await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })    

    console.log(`${onboarding}.invite from ${firstuser}`)
    await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })

    const vouchBeforeInvite = await eos.getTableRows({
        code: accounts,
        scope: newAccount,
        table: 'vouch',
        json: true
      })

    console.log(`${onboarding}.accept from Application`)
    await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })    

    const acceptUsers = await eos.getTableRows({
        code: accounts,
        scope: accounts,
        table: 'users',
        json: true
    })
    console.log("users "+JSON.stringify(acceptUsers, null, 2))

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const vouchAfterInvite = await eos.getTableRows({
        code: accounts,
        scope: newAccount,
        table: 'vouch',
        json: true
    })
    
    console.log("vouch after accept "+JSON.stringify(vouchAfterInvite, null, 2))

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
    console.log("New account "+newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
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

    console.log(`onboarding ${onboarding}.accept from Application for `+newAccount)
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
    console.log("New account "+newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    console.log(`reset ${accounts}`)
    await contracts.accounts.reset({ authorization: `${accounts}@active` })

    console.log(`reset ${onboarding}`)
    await contracts.onboarding.reset({ authorization: `${onboarding}@active` })

    console.log(`reset ${harvest}`)
    await contracts.harvest.reset({ authorization: `${harvest}@active` })    
  
    const adduser = async (user) => {
        try {
            console.log(`${accounts}.adduser (${user})`)
            await contracts.accounts.adduser(user, '', 'individual',{ authorization: `${accounts}@active` })        
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

    const accept = async () => {
        console.log(`${onboarding}.accept from Application`)
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })    
    }

    await adduser(firstuser)

    await adduser(newAccount)

    console.log('plant seeds')
    await contracts.token.transfer(firstuser, newAccount, '500.0000 SEEDS', '', { authorization: `${firstuser}@active` })
    await contracts.token.transfer(newAccount, harvest, '500.0000 SEEDS', '', { authorization: `${newAccount}@active` })
  
    const before = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    const beforeBalance = await getBalance(seconduser)

    await deposit(firstuser)

    await invite()

    await accept()

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    // console.log("before "+JSON.stringify(before, null, 2))
    // console.log("after "+JSON.stringify(rows, null, 2))


    //const afterBalance = await getBalance(seconduser)

    // console.log("before "+JSON.stringify(beforeBalance, null, 2))
    // console.log("after "+JSON.stringify(afterBalance, null, 2))

    const newUserHarvest = rows.find(row => row.account === newAccount)

    assert({
        given: 'invited new user',
        should: 'have planted seeds',
        actual: newUserHarvest,
        expected: {
            account: newAccount,
            planted: "505.0000 SEEDS",
            reward: '0.0000 SEEDS'
        }
    })
})


describe.only('Create bioregion', async assert => {

    if (!isLocal()) {
        console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
        return
    }
    
    const contracts = await initContracts({ onboarding, bioregion })
    
    const newAccount = randomAccountNameBDC()
    console.log("New account "+newAccount)
    const keyPair = await createKeypair()
    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public
  
    await contracts.onboarding.createbio(firstuser, newAccount, newAccountPublicKey,{ authorization: `${onboarding}@active` })        

    var hasNewDomain = false
    try {
      const accountInfo = await eos.getAccount(newAccount)
      hasNewDomain = true;
    } catch {
  
    }

    assert({
        given: 'created bioregion',
        should: 'have account',
        actual: hasNewDomain,
        expected: true
    })
})


