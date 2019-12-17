const { describe } = require('riteway')

const { eos, names, getTableRows, initContracts, sha256, isLocal, ramdom64ByteHexString, createKeypair } = require('../scripts/helper')

const { onboarding, token, accounts, harvest, firstuser } = names

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
            await contracts.accounts.adduser(firstuser, '', { authorization: `${accounts}@active` })        
        } catch (error) {
            console.log("user exists")
        }
    }

    const deposit = async () => {
        console.log(`${token}.transfer from ${firstuser} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })    
    }

    const invite = async () => {
        console.log(`${onboarding}.invite from ${firstuser}`)
        await contracts.onboarding.invite(firstuser, transferQuantity, sowQuantity, inviteHash, { authorization: `${firstuser}@active` })
    }

    const accept = async () => {
        console.log(`${onboarding}.accept from ${newAccount}`)
        await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })    
    }

    await reset()

    await adduser()

    await deposit()

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

describe('Use application permission to accept', async assert => {

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

    const adduser = async () => {
        try {
            console.log(`${accounts}.adduser (${firstuser})`)
            await contracts.accounts.adduser(firstuser, '', { authorization: `${accounts}@active` })        
        } catch (error) {
            console.log("user exists")
        }
    }

    const deposit = async () => {
        console.log(`${token}.transfer from ${firstuser} to ${onboarding} (${totalQuantity})`)
        await contracts.token.transfer(firstuser, onboarding, totalQuantity, '', { authorization: `${firstuser}@active` })    
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

    await deposit()

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
