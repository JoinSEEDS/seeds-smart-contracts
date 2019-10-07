#!/usr/bin/env node

const program = require('commander')
const compile = require('./compile')

program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    await compile({
      contract: contract,
      source: `./src/seeds.${contract}.cpp`
    })
    
    console.log(`${contract} compiled`)
  })
  
program.parse(process.argv)