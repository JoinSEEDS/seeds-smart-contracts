#!/usr/bin/env node


const program = require('commander')

const { eos, accounts, stakes } = require('./init')
const create = require('./create')(eos)
const test = require('./test')(eos)
const deploy = require('./deploy')({ eos, accounts })
const compile = require('./compile')

program
  .command('compile <contract>')
  .description('Compile custom contract')
  .action(async function (contract) {
    try {
      await compile(contract)
      console.log(`${contract} compiled`)
    } catch (err) {
      console.error(err)
    }
  })

program
  .command('deploy <contract>')
  .description('Create account & deploy contract')
  .action(async function (contract) {
    try {
      await create(accounts[contract])
    } catch (err) {
      console.error(err)
    }
  })
  
program
  .command('test <contract>')
  .description('Run unit tests for deployed contract')
  .action(async function(contract) {
    await test(contract)
  })
  
program
  .command('run <contract>')
  .description('Compile & Deploy & Test contract')
  .action(async function(contract) {
    try {
      await compile(contract)
      await create(accounts[contract])
      await deploy(contract)
      await test(contract)
    } catch (err) {
      console.error(err)
    }
  })
  
program.parse(process.argv)