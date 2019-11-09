const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, sha256 } = require('../scripts/helper')

const { onboarding, token, accounts, harvest, firstuser } = names

const randomAccountName = () => Math.random().toString(36).substring(2).replace(/\d/g, '').toString()

const fromHexString = hexString =>
  new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))

describe('Onboarding', async assert => {
    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `10.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '15.0000 SEEDS'
    
    const newAccount = randomAccountName()
    const newAccountPublicKey = 'EOS7iYzR2MmQnGga7iD2rPzvm5mEFXx6L1pjFTQYKRtdfDcG9NTTU'

    const inviteSecret = '9e7a0c24cb284ed7939e5d37901428fb1b293e56445c571a176fad2b948c0aaa'
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const reset = async () => {
        console.log(`reset ${accounts}`)
        await contracts.accounts.reset({ authorization: `${accounts}@active` })
    
        console.log(`reset ${onboarding}`)
        await contracts.onboarding.reset({ authorization: `${onboarding}@active` })
    
        console.log(`reset ${harvest}`)
        await contracts.harvest.reset({ authorization: `${harvest}@active` })    
    }

    const adduser = async () => {
        console.log(`${accounts}.adduser (${firstuser})`)
        await contracts.accounts.adduser(firstuser, '', { authorization: `${accounts}@active` })    
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