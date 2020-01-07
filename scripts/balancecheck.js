#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')
const { initContracts, resetByName } = require('./deploy')
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("./helper")
const { harvest } = names

const allContracts = [
  "accounts", 
  "policy", 
  "settings", 
  "token", 
  "harvest", 
  "proposals",
  "invites",
  "referendums",
  "history",
  "acctcreator",
  "exchange",
].sort()


const getbalance = async (account) => {
  try {
    let token = "token.seeds"
    let balance = await eos.getCurrencyBalance(token, account, 'SEEDS')
    if (balance == "") {
      console.log("Balance for "+account+": No balance")
    } else {
      console.log("Balance for "+account+": "+balance + " ("+token+")")
    }
    const plantedBalances = await getTableRows({
      code: harvest,
      scope: harvest,
      table: 'balances',
      lower_bound: account,
      upper_bound: account,
      json: true,
      limit: 100
    })
    console.log("Planted for "+account+": "+JSON.stringify(plantedBalances, null, 2))

  
  } catch (err) {
    console.log("error "+err)
  }
}

program
  .arguments('<account>')
  .description('Get SEEDS balance for an account')
  .name("balancecheck.js")
  .usage("<account>")
  .action(async function (account) {
    await getbalance(account)
  })

program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}
 
