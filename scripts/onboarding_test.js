const program = require('commander')
const fs = require('fs')

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



const bulk_invite = async (sponsor, num, totalAmount) => {
    var secrets = "Secret,Hash,Seeds (total)\n"
    const fileName = 'secrets_'+num+'.csv'
    var log = []

    try {
        totalAmount = parseInt(totalAmount)
        for (i = 0; i < num; i++) {
            console.log("inviting "+(i+1) + "/" + num)
            let inv = await invite(sponsor, totalAmount, false)
            console.log("secret "+inv.secret)
            console.log("hashed secret: "+inv.hashedSecret)
            log.push(inv)
            secrets = secrets + inv.secret +"," + inv.hashedSecret + "," + totalAmount + "\n"
        }
    
        console.log("secrets: "+secrets)
    
        console.log("log: " + JSON.stringify(log, null, 2))
    
    
        fs.writeFileSync(fileName, secrets)
        fs.writeFileSync('secrets_log_'+num+'.json', JSON.stringify(log, null,2))
        
        console.log(num + " secrets written to "+fileName)
    
    } catch (err) {
        console.log("Bulk Invite Error: "+err)

        fs.writeFileSync("error_"+fileName, secrets)
        fs.writeFileSync('error_secrets_log_'+num+'.json', JSON.stringify(log, null,2))

    }


}
  
const invite = async (sponsor, totalAmount, debug = false) => {
    
    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    totalAmount = parseInt(totalAmount)

    const plantedSeeds = 5
    const transferredSeeds = totalAmount - plantedSeeds

    const transferQuantity = transferredSeeds + `.0000 SEEDS`
    const sowQuantity = plantedSeeds + '.0000 SEEDS'
    const totalQuantity = totalAmount + '.0000 SEEDS'
    
    console.log("transferQuantity " + transferQuantity)
    console.log("sowQuantity " + sowQuantity)
    console.log("totalQuantity " + totalQuantity)

    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const result = {
        secret: inviteSecret,
        hashedSecret: inviteHash
    }

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

    if (debug == true) {
        const sponsorsBefore = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
        console.log("sponsors after deposit "+JSON.stringify(sponsorsBefore.rows, null, 2))    
    }

    await invite()

    if (debug == true) {
        const sponsorsAfter = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
        console.log("sponsors after invite "+JSON.stringify(sponsorsAfter.rows, null, 2))
    }   

    return result

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
            await contracts.onboarding.accept(newAccount, inviteSecret, newAccountPublicKey, { authorization: `${onboarding}@application` })        
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
  .command('invite <sponsor>')
  .description('invite new user')
  .action(async function (sponsor) {
      console.log("invite with " + sponsor)
    await invite(sponsor, 20, true) // always 20 seeds
  })

program
  .command('bulk_invite <sponsor> <num> <totalAmount>')
  .description('invite new user')
  .action(async function (param, num, totalAmount) {
      console.log("generate "+num+" invites at " + totalAmount + " SEEDS" + " with sponsor " + param)
    await bulk_invite(param, num, totalAmount)
  })


program
  .command('accept <newAccount> <inviteSecret>')
  .description('accept invite')
  .action(async function (newAccount, inviteSecret) {
    console.log("accept invite with " + newAccount + " sec: " + inviteSecret)
    await accept(newAccount, inviteSecret)
  })

program.parse(process.argv)