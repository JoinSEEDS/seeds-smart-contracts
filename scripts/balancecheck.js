#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')
const { initContracts, resetByName } = require('./deploy')
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal } = require("./helper")

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
    console.log("Balance for "+account+": "+balance + " ("+token+")")
  
  } catch (err) {
    console.log("error "+err)
  }
}

program
  .command('/d  ÷  <account>')
  .description('get balance')
  .action(async function (account) {
      console.log("invite with " + account)
    await getbalance(account)
  })

  program.parse(process.argv)