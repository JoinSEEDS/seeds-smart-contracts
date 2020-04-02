#!/usr/bin/env node

const program = require('commander')
const fs = require('fs')


const { eos, names, getTableRows, initContracts, sha256, isLocal, ramdom64ByteHexString, fromHexString, createKeypair } = require('../scripts/helper')

const { onboarding, token, accounts, harvest, firstuser } = names


const bulk_cancel = async (sponsor, hashList) => {
    const contracts = await initContracts({ onboarding })

    let try_again_hashes = []

    for (let i=0; i<hashList.length; i++) {

        let hash = hashList[i]

        console.log("cancel invite with hash "+hash)

        try {
            await contracts.onboarding.cancel(sponsor, hash, { authorization: `${sponsor}@active` } )
            
        } catch (err) {
            console.log("error canceling "+hash)
            console.log(err)
        }
    }
}

// max batched actions - 20 seemed to be stable, 44 would sometimes crash (not good, messy)
// and 100 would always crash on mainnet but work fine on testnet

const max_batched_actions = 20 

const bulk_invite = async (sponsor, num, totalAmount) => {

    totalAmount = parseInt(totalAmount)
    var secrets = "Secret,Hash,Seeds (total)\n"
    const fileName = 'secrets_'+num+'.csv'
    var log = []

    // deposit 
    const contracts = await initContracts({ token, onboarding })
    let depositAmount = num * totalAmount
    const plantedSeeds = 5
    const transferredSeeds = totalAmount - plantedSeeds
    const depositAmmountSeeds = parseInt(depositAmount) + '.0000 SEEDS'

    
    try {
        // create actions
        let actions = []
        actions.push( createCPUAction(sponsor) )


        // test the payforcpu action
        const transactionResult = await eos.transaction({
            actions: actions
          }, {
            blocksBehind: 3,
            expireSeconds: 30,
          });

        // todo make this transfer part of the transaction that adds the accounts!
        // in case something goes wrong!
        
        console.log("DISABLED comment transaction back in if you need to sponsor...")
        return
        // COMMENT THIS BACK IN
        //console.log("deposit "+depositAmmountSeeds)
        //await contracts.token.transfer(sponsor, onboarding, depositAmmountSeeds, '', { authorization: `${sponsor}@active` })        
      
        for (i = 0; i < num; i++) {
            let inv = await createInviteAction(sponsor, transferredSeeds, plantedSeeds)
    
            console.log("inviting "+(i+1) + "/" + num)
            console.log("secret "+inv.secret)
            console.log("hashed secret: "+inv.hashedSecret)    
    
            if (i > 0 && i % max_batched_actions == 0) {
                const transactionResult = await eos.transaction({
                    actions: actions
                  }, {
                    blocksBehind: 3,
                    expireSeconds: 30,
                  });
                
                  actions = []
                  actions.push( createCPUAction(sponsor) )

                  fs.writeFileSync(fileName+".tmp."+i, secrets)
                  fs.writeFileSync('invite_log_'+num+'.json', JSON.stringify(log, null,2))
      
            }

            actions.push(inv.action)
            log.push(inv)
            secrets = secrets + inv.secret +"," + inv.hashedSecret + "," + totalAmount + "\n"
        }
    
        console.log("secrets: "+secrets)
    
        console.log("actions: " + JSON.stringify(actions, null, 2))
    
        if (actions.length > 0) {
            const transactionResult = await eos.transaction({
                actions: actions
              }, {
                blocksBehind: 3,
                expireSeconds: 30,
              });
        }
        
    
        fs.writeFileSync(fileName, secrets)
        fs.writeFileSync('invite_log_'+num+'.json', JSON.stringify(log, null,2))
        
        console.log(num + " secrets written to "+fileName)
    
    } catch (err) {
        fs.writeFileSync("error_"+fileName, secrets)
        fs.writeFileSync('error_invite_log_'+num+'.json', JSON.stringify(log, null,2))

        console.log("Bulk Invite Error: "+err)
        //if (err instanceof RpcError) // this is in the latest version of eosjs - 20+
        //    console.log("RpcError in transact" + JSON.stringify(err.json, null, 2));

    }


}


const createInviteAction = async (sponsor, transfer, sow) => {
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const action = {
        account: 'join.seeds',
        name: 'invite',
        authorization: [{
          actor: sponsor,
          permission: 'active',
        }],
        data: {
            sponsor: sponsor,
            transfer_quantity: parseInt(transfer) + ".0000 SEEDS",
            sow_quantity: parseInt(sow) + '.0000 SEEDS',
            invite_hash: inviteHash,
        }
      }
    const result = {
        secret: inviteSecret,
        hashedSecret: inviteHash,
        action: action
    }
    return result   

}

const createCPUAction = (sponsor) => {
    return {
        account: harvest,
        name: 'payforcpu',
        authorization: [
            {
                actor: harvest,
                permission: 'payforcpu',
            },
            {
                actor: sponsor,
                permission: 'active',
            }
        ],
        data: {
            account: sponsor
        }
      }

}

const invite = async (sponsor, totalAmount, debug = false, depositFunds = false) => {
    
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

    if (depositFunds) {
        await deposit()
    }

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
        lower_bound: newAccount,
        upper_bound: newAccount,  
        json: true
    })

    console.log("Accept result: "+JSON.stringify(rows, null, 2))

}

program
  .command('invite <sponsor>')
  .description('invite new user')
  .action(async function (sponsor) {
      console.log("invite with " + sponsor)
    await invite(sponsor, 20, true) // always 20 seeds
  })

  // join.seeds before
//   memory: 
//      quota:     1.432 MiB    used:     879.7 KiB  

program
  .command('bulk_invite <sponsor> <num> <totalAmount>')
  .description('Bulk invite new users')
  .action(async function (param, num, totalAmount) {
      console.log("generate "+num+" invites at " + totalAmount + " SEEDS" + " with sponsor " + param)
    await bulk_invite(param, num, totalAmount)
  })

  program
  .command('bulk_cancel <sponsor> <filename>')
  .description('Bulk cancel invites from a file, each line contains an invite hash')
  .action(async function (sponsor, filename) {
    console.log("cancel invites from " + filename)
    var text = fs.readFileSync(filename)
    var textByLine = text.toString().split("\n")

    await bulk_cancel(sponsor, textByLine)
  })



  program
  .command('accept <newAccount> <inviteSecret>')
  .description('accept invite')
  .action(async function (newAccount, inviteSecret) {
    console.log("accept invite with " + newAccount + " sec: " + inviteSecret)
    await accept(newAccount, inviteSecret)
  })

  program
  .command('test <sponsor> <newAccount>')
  .description('test invite process')
  .action(async function (sponsor, newAccount) {
      
    console.log("invite from " + sponsor)
    let result = await invite(sponsor, 20, true) // always 20 seeds

    console.log("accept invite with " + newAccount + " secret: " + result.secret)
    await accept(newAccount, result.secret)
  })

program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}
