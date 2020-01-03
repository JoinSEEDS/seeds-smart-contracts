const program = require('commander')
const fs = require('fs')
const { names, getTableRows, initContracts } = require('../scripts/helper')

const { accounts, proposals } = names


const makecitizen = async (user, citizen = true) => {
    const contracts = await initContracts({ accounts, proposals })

    if (citizen) {
        await contracts.accounts.genesis(user, { authorization: `${accounts}@active` })
        console.log("success!")
        fs.appendFileSync('citizens.txt', user+"\n");
        
        console.log('add voice...')
        await contracts.proposals.addvoice(user, 77, { authorization: `${proposals}@active` })
      
    } else {
        await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
    }

}

const addvoice = async (user, contracts) => {      
  
    console.log('add voice for '+JSON.stringify(user, null, 2))
    await contracts.proposals.addvoice(user, 20, { authorization: `${proposals}@active` })
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

program
  .command('citizen <user>')
  .description('make new citizen out of user')
  .action(async function (user) {
      console.log("make " + user + " a citizen of SEEDS!")
    await makecitizen(user, true) 
})

program
  .command('resident <user>')
  .description('make new resident out of user')
  .action(async function (user) {
      console.log("make " + user + " a resident of SEEDS!")
    await makecitizen(user, false) 
})


program
  .command('initvoice')
  .description('init voice for all users')
  .action(async function () {
      console.log("init voice")
    await initvoice() 
})
program.parse(process.argv)