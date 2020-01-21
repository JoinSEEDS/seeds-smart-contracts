#!/usr/bin/env node

const program = require('commander')
const fs = require('fs')
const { names, getTableRows, initContracts } = require('../scripts/helper')

const { accounts, proposals } = names


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

program
  .arguments('<user>')
  .description('get information about user')
  .action(async function (user) {
    await checkuser(user) 
})


program.parse(process.argv)