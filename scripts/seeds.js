#!/usr/bin/env node

const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')

const compileAction = async (contract) => {
    try {
      await compile({
        contract: contract,
        source: `./src/seeds.${contract}.cpp`
      })
      console.log(`${contract} compiled`)
    } catch (err) {
        console.log("compile failef for ", contract, " error: ", err)
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
  
program.parse(process.argv)