#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')
const initContracts = require('./deploy')

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

  const arr = [
    "accounts", 
    "policy", 
    "settings", 
    "token", 
    "harvest", 
    "proposals", 
    "subscription"
  ]
  for (i=0; i<arr.length; i++) {
    let item = arr[i];
    console.log("compile ... " + item);
    await compileAction(item);
  }

  await initContracts()

}

program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    await compileAction(contract)
  })

program
  .command('deploy <contract>')
  .description('Deploy custom contract')
  .action(async function (contract) {
    await deployAction(contract)
  })

program
  .command('run <contract>')
  .description('compile and deploy custom contract')
  .action(async function (contract) {
    await runAction(contract)
  })

program
  .command('test <contract>')
  .description('Run unit tests for deployed contract')
  .action(async function(contract) {
    await test(contract)
  })

program
  .command('init')
  .description('Initial creation of all accounts and contracts contract')
  .action(async function(contract) {
    await initAction()
  })

  
program.parse(process.argv)