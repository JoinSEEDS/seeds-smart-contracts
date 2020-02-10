const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, sha256, isLocal, ramdom64ByteHexString, createKeypair, getBalance } = require('../scripts/helper')

const { onboarding, token, accounts, harvest, firstuser, seconduser } = names

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
    console.log("New account "+newAccount)

    const keyPair = await createKeypair()

    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))

    const newAccountPublicKey = keyPair.public

    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const inviteSecret2 = await ramdom64ByteHexString()
    const inviteHash2 = sha256(fromHexString(inviteSecret2)).toString('hex')

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
            await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })        
        } catch (error) {
            console.log("user exists")
        }
    }

    const deposit = async (user) => {
        console.log(`${token}.transfer from ${user} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(user, onboarding, totalQuantity, '', { authorization: `${user}@active` })    
    }

    const invite = async (user, inviteHash) => {
        console.log(`${onboarding}.invite from ${user}`)
        await contracts.onboarding.invite(user, transferQuantity, sowQuantity, inviteHash, { authorization: `${user}@active` })
    }

    const accept = async () => {
        console.log(`${onboarding}.accept from ${newAccount}`)
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })    
    }

    await reset()

    await adduser()

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
        given: 'search by sponsor user 2',
        should: 'contains sponsor',
        actual: invitesBySponsor.rows[0].sponsor,
        expected: seconduser
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

    const adduser = async () => {
        try {
            console.log(`${accounts}.adduser (${firstuser})`)
            await contracts.accounts.adduser(firstuser, '', 'individual', { authorization: `${accounts}@active` })        
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

    await adduser()

    await deposit(firstuser)

    await invite()

    const vouchBeforeInvite = await eos.getTableRows({
        code: accounts,
        scope: newAccount,
        table: 'vouch',
        json: true
      })

    await accept()

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

    const accept = async () => {
        console.log(`${onboarding}.accept from Application`)
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })    
    }

    await reset()

    await deposit(firstuser)

    await invite()

    await accept()

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

    await reset()


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

    console.log("before "+JSON.stringify(before, null, 2))
    console.log("after "+JSON.stringify(rows, null, 2))


    const afterBalance = await getBalance(seconduser)

    console.log("before "+JSON.stringify(beforeBalance, null, 2))
    console.log("after "+JSON.stringify(afterBalance, null, 2))

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


