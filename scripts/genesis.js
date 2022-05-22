#!/usr/bin/env node

const program = require('commander')
const fs = require('fs')
const { names, getTableRows, initContracts } = require('../scripts/helper')

const { accounts, proposals } = names


const makecitizen = async (user, citizen = true) => {
    console.log("** makec "+ user)

    const contracts = await initContracts({ accounts, proposals })

    const users = await getTableRows({
      code: accounts,
      scope: accounts,
      table: 'users',
      lower_bound: user,
      upper_bound: user,
      json: true,
    })

    const voice = await getTableRows({
      code: proposals,
      scope: proposals,
      table: 'voice',
      lower_bound: user,
      upper_bound: user,
      json: true,
    })

    let alreadyHasVoice = voice.rows.length > 0

    if (alreadyHasVoice) {
      console.log("user " + user + " already has voice!")
    }

    if (users.rows.length != 1) {
      console.log("account "+user +" is not a Seeds user")
      return
    } 

    let status = users.rows[0].status
    if (status == "citizen" | status == "resident") {
      console.log("account "+ user +" is already a "+status + "!")
      console.log(" "+JSON.stringify(users, null, 2))
      return
    }  

    if (citizen) {
        await contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })
        console.log("success!")
        fs.appendFileSync('citizens.txt', user+"\n");
        
        if (alreadyHasVoice) {
          console.log("user already has voice: "+JSON.stringify(voice, null, 2))
        } else {
          console.log('add voice...')
          await contracts.proposals.addvoice(user, 1, { authorization: `${proposals}@active` })  
        }
      
    } else {
        await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
    }

}

const checkuser = async (user) => {
  const contracts = await initContracts({ accounts, proposals })

  const users = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    lower_bound: user,
    upper_bound: user,
    json: true,
  })

  const voice = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    lower_bound: user,
    upper_bound: user,
    json: true,
  })

  console.log("User "+user)

  console.log("acount "+JSON.stringify(users, null, 2))
  console.log("voice "+JSON.stringify(voice, null, 2))

}

const addvoice = async (user, contracts) => {      
  
    console.log('add voice for '+JSON.stringify(user, null, 2))
    await contracts.proposals.addvoice(user, 77, { authorization: `${proposals}@active` })
    console.log("voice added for "+user+".")
}

const initvoice = async (user) => {

    const contracts = await initContracts({ accounts, proposals })

    const users = await getTableRows({
        code: accounts,
        scope: accounts,
        table: 'users',
        json: true,
        limit: 300
      })

      for(var i=0; i<users.rows.length; i++) {
          let user = users.rows[i].account
          console.log(i + " testing user "+user + " status: "+users.rows[i].status)
          if (users.rows[i].status == "citizen") {
            console.log('add voice for '+JSON.stringify(user, null, 2))
            await addvoice(user, contracts)
          }
        }
}

const deleteuser = async (user) => {

  console.log("Delete is disabled for safety")
  return

  const contracts = await initContracts({ accounts, proposals })

  const users = await getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    lower_bound: user,
    upper_bound: user,
    json: true,
  })

  const voice = await getTableRows({
    code: proposals,
    scope: proposals,
    table: 'voice',
    lower_bound: user,
    upper_bound: user,
    json: true,
  })

  let alreadyHasVoice = voice.rows.length > 0

  if (alreadyHasVoice) {
    console.log("user " + user + " has voice! - currently no way to delete that")
  }

  if (users.rows.length != 1) {
    console.log("account "+user +"is not a Seeds user")
    return
  } 

  await contracts.accounts.testremove(user, { authorization: `${accounts}@active` })
  console.log("success!")
      
  if (alreadyHasVoice) {
    console.log("user already has voice: "+JSON.stringify(voice, null, 2))
  } 
    
 

}


program
  .command('citizen <user>')
  .description('make new citizen out of user')
  .action(async function (user) {
      console.log("make " + user + " a citizen of SEEDS!")
    await makecitizen(user.toLowerCase(), true) 
})

program
  .command('bulk_add <file>')
  .description('read usernames from file, make citizens')
  .action(async function (file) {
    console.log("make all users in " + file + " citizens of SEEDS!")
    var text = fs.readFileSync(file)
    var textByLine = text.toString().split("\n")
    for (let i =0; i<textByLine.length; i++) {
      let acct = textByLine[i]
      if (acct.length > 0) {
        await makecitizen(textByLine[i].toLowerCase(), true)
      }
    }
    //textByLine.forEach( async (user) => { await makecitizen(user, true) } )

  })

program
  .command('resident <user>')
  .description('make new resident out of user')
  .action(async function (user) {
      console.log("make " + user + " a resident of SEEDS!")
    await makecitizen(user, false) 
})

program
  .command('check <user>')
  .description('get information about user')
  .action(async function (user) {
    await checkuser(user) 
})

program
  .command('DELETE <user>')
  .description('delete a user')
  .action(async function (user) {
    await deleteuser(user) 
})



program
  .command('initvoice')
  .description('init voice for all users')
  .action(async function () {
      console.log("init voice")
    await initvoice() 
})
program.parse(process.argv)