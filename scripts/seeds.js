#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const { isLocal } = require('./helper')
const deploy = require('./deploy.command')
const { initContracts, resetByName } = require('./deploy')

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
  "onboarding",
  "forum",
  "scheduler"
].sort()


const compileAction = async (contract) => {
    try {
      await compile({
        contract: contract,
        source: `./src/seeds.${contract}.cpp`
      })
      console.log(`${contract} compiled`)
    } catch (err) {
        console.log("compile failed for " + contract + " error: " + err)
    }
}

const deployAction = async (contract) => {
    try {
      await deploy(contract)
      console.log(`${contract} deployed`)
    } catch(err) {
      console.log("error deploying ", contract)
      console.log(err)
    }
}

const resetAction = async (contract) => {
  if (contract == "history") {
    console.log("history can't be reset, skipping...")
    console.log("TODO: Add reset action for history that resets all tables")
    return
  }

  if (!isLocal()) {
    console.log("Don't reset contracts on testnet or mainnet!")
    return
  }

  try {
    await resetByName(contract)
    console.log(`${contract} reset`)
  } catch(err) {
    console.log("error deploying ", contract)
    console.log(err)
  }
}

const runAction = async (contract) => {
  await compileAction(contract)
  await deployAction(contract)
  await test(contract)
}

const batchCallFunc = async (contract, func) => {
  if (contract == 'all') {
    for (const contract of allContracts) {
      await func(contract)
    }
  } else {
    await func(contract)
  }
}

const initAction = async () => {

  for (i=0; i<allContracts.length; i++) {
    let item = allContracts[i];
    console.log("compile ... " + item);
    await compileAction(item);
  }

  await initContracts()

}

program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    await batchCallFunc(contract, compileAction)
  })

program
  .command('deploy <contract>')
  .description('Deploy custom contract')
  .action(async function (contract) {
    await batchCallFunc(contract, deployAction)
  })

program
  .command('run <contract>')
  .description('compile and deploy custom contract')
  .action(async function (contract) {
    await batchCallFunc(contract, runAction)
  })

  program
  .command('test <contract>')
  .description('Run unit tests for deployed contract')
  .action(async function(contract) {
    await batchCallFunc(contract, test)
  })

  program
  .command('reset <contract>')
  .description('Reset deployed contract')
  .action(async function(contract) {
    await batchCallFunc(contract, resetAction)
  })

program
  .command('init')
  .description('Initial creation of all accounts and contracts contract')
  .action(async function(contract) {
    await initAction()
  })

  
program.parse(process.argv)