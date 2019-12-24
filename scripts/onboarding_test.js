const program = require('commander')

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
 
const fromHexString = hexString => new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))




  
const invite = async (sponsor, sponsorActiveKey) => {
    
    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    const transferQuantity = `5.0000 SEEDS`
    const sowQuantity = '5.0000 SEEDS'
    const totalQuantity = '10.0000 SEEDS'
    
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')



    console.log("Invite Secret: "+inviteSecret)
    console.log("Secret hash: "+inviteHash)

    
    const deposit = async () => {
        try {
            console.log(`${token}.transfer from ${sponsor} to ${onboarding} (${totalQuantity})`)
            await contracts.token.transfer(sponsor, onboarding, totalQuantity, '', { authorization: `${sponsor}@active` })        
        } catch (err) {
            console.log("deposit error: " + err)
        }
    }

    const invite = async () => {
        try {
            console.log(`${onboarding}.invite from ${sponsor}`)
            await contracts.onboarding.invite(sponsor, transferQuantity, sowQuantity, inviteHash, { authorization: `${sponsor}@active` })    
        } catch(err) {
            console.log("inv err: "+err)
        }
    }

    await deposit()

    const sponsorsBefore = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'sponsors',
        json: true
    })
    console.log("sponsors after deposit "+JSON.stringify(sponsorsBefore.rows, null, 2))

    await invite()

    const sponsorsAfter = await getTableRows({
        code: onboarding,
        scope: onboarding,
        table: 'sponsors',
        json: true
    })
    console.log("sponsors after invite "+JSON.stringify(sponsorsAfter.rows, null, 2))

}

const accept = async (newAccount, inviteSecret) => {

    const contracts = await initContracts({ onboarding, token, accounts, harvest })
    const keyPair = await createKeypair()
    console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
    const newAccountPublicKey = keyPair.public

    console.log("Onboarding: " + JSON.stringify(onboarding))


    const accept = async () => {
        try {
            console.log(`${onboarding}.accept from ${newAccount}`)
            await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@active` })        
        } catch (err) {
            console.log("Error accept: " + err)
        }
    }

    await accept()

    const { rows } = await getTableRows({
        code: harvest,
        scope: harvest,
        table: 'balances',
        json: true
    })

    console.log("Accept result: "+JSON.stringify(rows))

}

program
  .command('invite <param>')
  .description('invite new user')
  .action(async function (param) {
      console.log("invite with " + param)
    await invite(param)
  })

program
  .command('accept <newAccount> <inviteSecret>')
  .description('accept invite')
  .action(async function (newAccount, inviteSecret) {
    console.log("accept invite with " + newAccount + " sec: " + inviteSecret)
    await accept(newAccount, inviteSecret)
  })

program.parse(process.argv)