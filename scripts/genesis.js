const program = require('commander')
const fs = require('fs')
const { names, getTableRows, initContracts } = require('../scripts/helper')

const { accounts } = names

const makecitizen = async (user, citizen = true) => {
    const contracts = await initContracts({ accounts })
    if (citizen) {
        await contracts.accounts.genesis(user, { authorization: `${accounts}@active` })
    } else {
        await contracts.accounts.testresident(user, { authorization: `${accounts}@active` })
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

program.parse(process.argv)