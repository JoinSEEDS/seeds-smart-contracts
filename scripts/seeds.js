#!/usr/bin/env node

const program = require('commander')
const compile = require('./compile')
const deploy = require('./deploy.command')

program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    try {
      await compile({
        contract: contract,
        source: `./src/seeds.${contract}.cpp`
      })
      console.log(`${contract} compiled`)
    } catch (err) {
        console.log("compile failef for ", contract, " error: ", err)
    }
  })

program
  .command('deploy <contract>')
  .description('Deploy custom contract')
  .action(async function (contract) {
    try {
      await deploy(contract)
      console.log(`${contract} deployed`)
    } catch(err) {
      console.log("error deploying ", contract)
      console.log(err)
    }
  })

  program
  .command('run <contract>')
  .description('compile and deploy custom contract')
  .action(async function (contract) {
    try {
      await compile({
        contract: contract,
        source: `./src/seeds.${contract}.cpp`
      })
      console.log(`${contract} compiled`)

      await deploy(contract)
      console.log(`${contract} deployed`)
    
    } catch(err) {
      console.log("error running ", contract)
      console.log(err)
    }
  })
  
program.parse(process.argv)