#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')
const initContracts = require('./deploy')

const allContracts = [
  "accounts", 
  "policy", 
  "settings", 
  "token", 
  "harvest", 
  "proposals", 
  "subscription"
]

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

const runAction = async (contract) => {
  await compileAction(contract)
  await deployAction(contract)
  await test(contract)
}

const initAction = async () => {

  for (i=0; i<allContracts.length; i++) {
    let item = allContracts[i];
    console.log("compile ... " + item);
    await compileAction(item);
  }

  await initContracts()

}

const testAllAction = async () => {

  for (i=0; i<allContracts.length; i++) {
    let item = allContracts[i];
    console.log("testing " + item);
    await test(item)
  }

}

const deployAllAction = async () => {
  
  for (i=0; i<allContracts.length; i++) {
    let item = allContracts[i];
    console.log("deploying " + item);
    await deployAction(item)
  }

}

const compileAllAction = async () => {
  
  for (i=0; i<allContracts.length; i++) {
    let item = allContracts[i];
    console.log("compiling " + item);
    await compileAction(item)
  }

}

const runAllAction = async () => {
  
  await compileAllAction()
  await deployAllAction()
  await testAllAction()
  
}


program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    if (contract == "all") {
      compileAllAction()
    } else {
      await compileAction(contract)
    }
  })

program
  .command('deploy <contract>')
  .description('Deploy custom contract')
  .action(async function (contract) {
    if (contract == "all") {
      await deployAllAction()
    } else {
      await deployAction(contract)
    }
  })

program
  .command('run <contract>')
  .description('compile and deploy custom contract')
  .action(async function (contract) {
    if (contract == "all") {
      runAllAction()
    } else {
      await runAction(contract)
    }
  })

program
  .command('test <contract>')
  .description('Run unit tests for deployed contract')
  .action(async function(contract) {
    if (contract == "all") {
      await testAllAction()
    } else {
      await test(contract)
    }
  })

program
  .command('init')
  .description('Initial creation of all accounts and contracts contract')
  .action(async function(contract) {
    await initAction()
  })

  
program.parse(process.argv)