#!/usr/bin/env node

// NOTE: For any of these to work, add the sponsor private key to the helper.js file  here

// ...
// const keyProviders = {
// ...
//   [networks.telosMainnet]: ["ADD YOUR SPONSOR PRIVATE KEY HERE", process.env.TELOS_MAINNET_OWNER_KEY, process.env.TELOS_MAINNET_ACTIVE_KEY, process.env.APPLICATION_KEY, process.env.EXCHANGE_KEY,process.env.PAY_FOR_CPU_MAINNET_KEY,process.env.SCRIPT_KEY],
// ...
// }
// ...

const program = require('commander')
const fs = require('fs')

const { eos, names, getTableRows, initContracts, sha256, isLocal, ramdom64ByteHexString, fromHexString, createKeypair } = require('../scripts/helper')
const { worker } = require('cluster')
const helper = require('../scripts/helper')
const { createESRWithActions } = require('./msig')

const { onboarding, token, accounts, organization, harvest, firstuser } = names

const cancel = async (sponsor, hash) => {
  await bulk_cancel(sponsor, [hash])
  console.log("canceled "+hash)
}

const bulk_cancel = async (sponsor, hashList) => {
    const contracts = await initContracts({ onboarding })
    
    let actions = []
    actions.push( createCPUAction(sponsor) )

    let max_batched_actions = 27

    for (let i=0; i<hashList.length; i++) {

        let hash = hashList[i]

        let line = hash.split(",")
        if (line.length>1) {
            hash = line[1]
        }
        if (hash.length < 20) {
            continue
        }
        
        console.log((i+1)+ "/" +hashList.length + " cancel invite with hash "+hash)

        let action = createCancelAction(sponsor, hash)

        if (i > 0 && i % max_batched_actions == 0) {

            const transactionResult = await eos.transaction({
                actions: actions
              }, {
                blocksBehind: 3,
                expireSeconds: 30,
              });
            
              actions = []
              actions.push( createCPUAction(sponsor) )

              console.log("Cancelled through "+i)
  
        }

        actions.push(action)

    }

    if (actions.length > 0) {
        console.log("Canceling remaining "+actions.length + " actions")
        const transactionResult = await eos.transaction({
            actions: actions
          }, {
            blocksBehind: 3,
            expireSeconds: 30,
          });
    }

}

const moon_phase = async (list, start = 0) => {
  
  let actions = []

  let max_batched_actions = 100

  for (let i=start; i<list.length; i++) {
      
      let fullLine = list[i]

      let line = fullLine.split(",")
      
      console.log((i+1)+ "/" +list.length + " adding phase ", line)

      // note: there's a time string in line[0] but we ignore that
      let action = createMoonPhaseAction({
        timestamp: line[1],
        name: line[2], 
        eclipse: line[3]
      })

      if (i > 0 && i % max_batched_actions == 0) {

        console.log("pushing actions... ")

        const transactionResult = await eos.transaction({
              actions: actions
            }, {
              blocksBehind: 3,
              expireSeconds: 30,
            });
          
            actions = []

            console.log("Moon phase through "+i)

      }

      actions.push(action)

  }

  if (actions.length > 0) {
      console.log("Adding remaining "+actions.length + " moon phases")
      const transactionResult = await eos.transaction({
          actions: actions
        }, {
          blocksBehind: 3,
          expireSeconds: 30,
        });
  }

}

// max batched actions - 20 seemed to be stable, 44 would sometimes crash (not good, messy)
// and 100 would always crash on mainnet but work fine on testnet

const max_batched_actions = 20 

const bulk_invite = async (sponsor, referrer, num, totalAmount) => {

    totalAmount = parseInt(totalAmount)
    var secrets = "Secret,Hash,Seeds (total)\n"
    var invitelist = ""
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
        
        //console.log("DISABLED comment transaction back in if you need to sponsor...")
        //return
        // COMMENT THIS BACK IN
        console.log("deposit "+depositAmmountSeeds)
        await contracts.token.transfer(sponsor, onboarding, depositAmmountSeeds, '', { authorization: `${sponsor}@active` })        
      
        for (i = 0; i < num; i++) {
            let inv = await createInviteAction(sponsor, referrer, transferredSeeds, plantedSeeds)
    
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
            invitelist = invitelist +"https://joinseeds.app.link/accept-invite?invite-secret="+inv.secret+"\n"
        }
    
        //console.log("secrets: "+secrets)
    
        //console.log("actions: " + JSON.stringify(actions, null, 2))
    
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
        fs.writeFileSync('invitelist_'+num+'.txt', invitelist)          
        console.log(num + " secrets written to "+fileName)
    
    } catch (err) {
        fs.writeFileSync("error_"+fileName, secrets)
        fs.writeFileSync('error_invite_log_'+num+'.json', JSON.stringify(log, null,2))

        console.log("Bulk Invite Error: "+err)
        //if (err instanceof RpcError) // this is in the latest version of eosjs - 20+
        //    console.log("RpcError in transact" + JSON.stringify(err.json, null, 2));

    }


}


const createInviteAction = async (sponsor, referrer, transfer, sow) => {
    const inviteSecret = await ramdom64ByteHexString()
    const inviteHash = sha256(fromHexString(inviteSecret)).toString('hex')

    const action = {
        account: 'join.seeds',
        name: 'invitefor',
        authorization: [{
          actor: sponsor,
          permission: 'active',
        }],
        data: {
            sponsor: sponsor,
            referrer: referrer,
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
const createCancelAction = (sponsor, hash) => {

    const action = {
        account: 'join.seeds',
        name: 'cancel',
        authorization: [{
          actor: sponsor,
          permission: 'active',
        }],
        data: {
            sponsor: sponsor,
            invite_hash: hash,
        }
      }
      return action
}

const createMoonPhaseAction = ({timestamp, name, eclipse}) => {

  const action = {
      account: 'cycle.seeds',
      name: 'moonphase',
      authorization: [{
        actor: 'cycle.seeds',
        permission: 'active',
      }],
      data: {
        timestamp: timestamp,
        phase_name: name,
        eclipse: eclipse
      }
    }
    return action
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
        //console.log("sponsors after deposit "+JSON.stringify(sponsorsBefore.rows, null, 2))    
    }

    await invite()

    if (debug == true) {
        const sponsorsAfter = await getTableRows({
            code: onboarding,
            scope: onboarding,
            table: 'sponsors',
            json: true
        })
        //console.log("sponsors after invite "+JSON.stringify(sponsorsAfter.rows, null, 2))
    }   

    return result

}

const accept = async (newAccount, inviteSecret, newAccountPublicKey = null) => {

    const contracts = await initContracts({ onboarding, token, accounts, harvest })

    if (newAccountPublicKey == null) {
      const keyPair = await createKeypair()
      console.log("new account keys: "+JSON.stringify(keyPair, null, 2))
      newAccountPublicKey = keyPair.public  
    }

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

const traceorgs = async () => {

    const refs = await eos.getTableRows({
        code: accounts,
        scope: accounts,
        table: 'refs',
        json: true,
        limit: 1000
      })
      const orgs = await eos.getTableRows({
        code: organization,
        scope: organization,
        table: 'organization',
        json: true,
        limit: 1000
      })
    
      const getAcct = async (user) => {
        const { rows } = await eos.getTableRows({
            code: accounts,
            scope: accounts,
            table: 'users',
            lower_bound: user,
            upper_bound: user,
            json: true,
            limit: 1
        })

        return rows[0].nickname
    
      }

      console.log(orgs.rows.length + " orgs")
      console.log(refs.rows.length + " refs")

      let refmap = {}
      refs.rows.forEach(item => {
          refmap[""+item.invited] = item.referrer
      });

      var output = ""

      for(i=0; i<orgs.rows.length; i++) {
        item = orgs.rows[i]
        let name = await getAcct(item.org_name)
        output += "Org: "+item.org_name + ": " + name + "\n"
        output += "Owner: "+item.owner + "\n"
        output += "Ambassador: " + refmap[item.owner] + "\n"
        output += "\n"
      }

      console.log(output)
}

const escrowBalance = async (account) => {
  console.log("Escrow Summary for "+account)
  console.log("====================================")

  var total = 0.0
  var more = true
  var next = 0
  while(more) {
    const escrow = await eos.getTableRows({
      code: "escrow.seeds",
      scope: "escrow.seeds",
      table: 'locks',
      lower_bound: next,
      json: true,
      limit: 1000
    })

    more = escrow.more
    next = escrow.next_key

    //console.log(" more "+ more + " next "+next + " rows: "+escrow.rows.length)

    escrow.rows.forEach(row => {
      if (row.beneficiary == account) {
        console.log("+ "+row.quantity + "\t\t date: " + row.created_date)
        total += parseFloat(row.quantity)
      }
    })
  }
  console.log("====================================")
  console.log("Total: "+total)


}
program
  .command('invite <sponsor>')
  .description('invite new user')
  .action(async function (sponsor) {
      console.log("invite with " + sponsor)
      let result = await invite(sponsor, 5, true, true) // always 5 seeds
      console.log("create invite: " + JSON.stringify(result, null, 2))
  })

  program
  .command('cancel <sponsor> <hash>')
  .description('cancel an invite')
  .action(async function (sponsor, hash) {
    console.log("cancel from " + sponsor)
    await cancel(sponsor, hash)
  })

  // join.seeds before
//   memory: 
//      quota:     1.432 MiB    used:     879.7 KiB  

program
  .command('bulk_invite <sponsor> <referrer> <numberOfInvites> <numberOfSeedsPerInvite>')
  .description('Bulk invite new users')
  .action(async function (sponsor, referrer, numberOfInvites, numberOfSeedsPerInvite) {
      console.log("generate "+numberOfInvites+" invites at " + numberOfSeedsPerInvite + " SEEDS" + " with sponsor " + sponsor)
      console.log("referrer: "+referrer)
    await bulk_invite(sponsor, referrer, numberOfInvites, numberOfSeedsPerInvite)
  })

  program
  .command('invite <sponsor>')
  .description('create 1 invite')
  .action(async function (sponsor) {
      
    console.log("invite from " + sponsor)
    let result = await invite(sponsor, 10, true, true) 

    console.log("created invite with  secret: " + JSON.stringify(result, null, 2));
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
  .command('moonphase <filename> <start>')
  .description('Add moon phases from a file')
  .action(async function (filename, start) {
    start = start ?? 0
    console.log("moon phases from " + filename + " start at " + start)
    var text = fs.readFileSync(filename)
    var textByLine = text.toString().split("\n")

    await moon_phase(textByLine, start)
  })


  program
  .command('accept <newAccount> <inviteSecret>')
  .description('accept invite')
  .action(async function (newAccount, inviteSecret) {
    console.log("accept invite with " + newAccount + " sec: " + inviteSecret)
    await accept(newAccount, inviteSecret)
  })

  program
  .command('test <sponsor> <newAccount> <newPublicKey>')
  .description('test invite process')
  .action(async function (sponsor, newAccount, newPublicKey) {
      
    console.log("invite from " + sponsor)
    let result = await invite(sponsor, 5, true, true) // always 20 seeds

    console.log("accept invite with " + newAccount + " secret: " + result.secret + " pub: " + newPublicKey)
    await accept(newAccount, result.secret, newPublicKey)
  })

  program
  .command('onboard_telos <sponsor> <amount>')
  .description('Onboard existing Telos users')
  .action(async function (sponsor, amount) {
      
    console.log("onboard existing telos user from " + sponsor)
    
    // TODO create QR code for invite and transfer
    // Then we have 1 QR code for creating the secret on chain
    // And 1 QR code for accepting an existing account
    let result = await invite(sponsor, amount, true, true)
        
    const authPlaceholder = "............1"

    const secret = result.secret

    const res = await createESRWithActions({ 
      actions: [
      {
        "account": "join.seeds",
        "name": "acceptexist",
        "authorization": [{
            "actor": authPlaceholder,
            "permission": "active"
          }
        ],
        "data": {
          "account": authPlaceholder,
          "invite_secret": secret,
        },
      }
    ]})
    console.log("accept invite ")
    console.log(JSON.stringify(res, null, 2))
    
  })

  program
  .command('invite_cancel <sponsor>')
  .description('test invite process')
  .action(async function (sponsor) {
      
    console.log("invite from " + sponsor)
    let result = await invite(sponsor, 5, true, true) // always 5 seeds

    console.log("cancel invite with  result: " + JSON.stringify(result, null, 2))
    await cancel(sponsor, result.hashedSecret)
  })

  program
  .command('orgs')
  .description('information about organizations')
  .action(async function () {
    await traceorgs() 
  })

  program
  .command('escrow <account>')
  .description('escrow balance')
  .action(async function (account) {
    await escrowBalance(account) 
  })

program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}
