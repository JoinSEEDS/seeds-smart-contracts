#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const { isLocal, initContracts: initContractsHelper, names } = require('./helper')
const { harvest } = names

const deploy = require('./deploy.command')
const { initContracts, updatePermissions, resetByName } = require('./deploy')

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
  "scheduler",
  "acctcreator",
  "exchange",
  "organization"
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
      let errStr = "" + err
      if (errStr.includes("Contract is already running this version of code")) {
        console.log(`${contract} code was already deployed`)
      } else {
        console.log("error deploying ", contract)
        console.log(err)          
      }
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
    let errStr = "" + err
    if (errStr.includes("Contract is already running this version of code")) {
      console.log(`${contract} code was already deployed`)
    } else {
      console.log("error deploying ", contract)
      console.log(err)          
    }
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

const updatePermissionAction = async () => {
  await updatePermissions()
}

const startHarvestCalculations = async () => {
  console.log("Starting harvest calculations...")
  const contracts = await initContractsHelper({ harvest })

  console.log("start calcplanted")
  await contracts.harvest.calcplanted({ authorization: `${harvest}@active` })
  console.log("start calcrep")
  await contracts.harvest.calcrep({ authorization: `${harvest}@active` })
  console.log("start calctrx")
  await contracts.harvest.calctrx({ authorization: `${harvest}@active` })
  console.log("done.")

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

  program
  .command('updatePermissions')
  .description('Update all permissions of all contracts')
  .action(async function(contract) {
    await updatePermissionAction()
  })

  program
  .command('startHarvestCalculations')
  .description('Start calculations on harvest contract')
  .action(async function(contract) {
    await startHarvestCalculations()
  })


  
program.parse(process.argv)