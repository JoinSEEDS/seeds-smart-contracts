#!/usr/bin/env node

const test = require('./test')
const program = require('commander')
const compile = require('./compile')
const { eos, isLocal, names, accounts, allContracts, allContractNames, allBankAccountNames } = require('./helper')
const docsgen = require('./docsgen')
const { settings, scheduler } = names

const deploy = require('./deploy.command')
const { deployAllContracts, updatePermissions, resetByName, changeOwnerAndActivePermission, changeExistingKeyPermission, createTestToken } = require('./deploy')


const getContractLocation = (contract) => {
  if (contract == "msig") {
    return {
      source: `../msig/src/${contract}.cpp`,
      include: `../msig/include`
    }
  } else {
    return {
      source: `./src/seeds.${contract}.cpp`,
      include: ""
    }
  }
}

const compileAction = async (contract) => {
    try {
      var { source, include } = getContractLocation(contract)
      // await compile({
      //   contract: contract,
      //   source,
      //   include
      // })
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
      let errStr = ("" + err).toLowerCase()
      if (errStr.includes("contract is already running this version of code")) {
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
    let errStr = ("" + err).toLowerCase()
    if (errStr.includes("contract is already running this version of code")) {
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
  //await test(contract)
}

const batchCallFunc = async (contract, moreContracts, func) => {
  if (contract == 'all') {
    for (const contract of allContracts) {
      await func(contract)
    }
  } else {
    await func(contract)
  }
  if (moreContracts) {
    for (var i=0; i<moreContracts.length; i++) {
      await func(moreContracts[i])
    }
  }
}

const initAction = async (compile = true) => {

  if (compile) {
    for (i=0; i<allContracts.length; i++) {
      let item = allContracts[i];
      console.log("compile ... " + item);
      await compileAction(item);
    }
  } else {
    console.log("no compile")
  }

  await deployAllContracts()

}

const updatePermissionAction = async () => {
  await updatePermissions()
}

const updateSettingsAction = async () => {
  console.log(`UPDATE Settings on ${settings}`)
  const name = "settings"
  
  await deployAction(name)

  const contract = await eos.contract(settings)

  console.log(`reset settings`)

  await contract.reset({ authorization: `${settings}@active` })

  console.log(`Success: Settings reset: ${settings}`)
}

const updateSchedulerAction = async () => {
  console.log(`UPDATE Scheduler on ${scheduler}`)
  const name = "scheduler"

  await deployAction(name)

  const contract = await eos.contract(scheduler)

  console.log(`${scheduler} stop`)
  await contract.stop({ authorization: `${scheduler}@active` })

  console.log(`${scheduler} updateops`)
  await contract.updateops({ authorization: `${scheduler}@active` })

  console.log(`${scheduler} start`)
  await contract.start({ authorization: `${scheduler}@active` })

  console.log(`Success: Scheduler was updated and restarted: ${scheduler}`)
}

program
  .command('compile <contract> [moreContracts...]')
  .description('Compile custom contract')
  .action(async function (contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, compileAction)
  })

program
  .command('deploy <contract> [moreContracts...]')
  .description('Deploy custom contract')
  .action(async function (contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, deployAction)
  })

program
  .command('run <contract> [moreContracts...]')
  .description('compile and deploy custom contract')
  .action(async function (contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, runAction)
  })

program
  .command('test <contract> [moreContracts...]')
  .description('Run unit tests for deployed contract')
  .action(async function(contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, test)
  })

program
  .command('reset <contract> [moreContracts...]')
  .description('Reset deployed contract')
  .action(async function(contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, resetAction)
  })

program
  .command('init [compile]')
  .description('Initial creation of all accounts and contracts contract')
  .action(async function(compile) {
    var comp = compile != "false" 
    await initAction(comp)
  })

  program
  .command('updatePermissions')
  .description('Update all permissions of all contracts')
  .action(async function() {
    await updatePermissionAction()
  })

  program
  .command('updateSettings')
  .description('Deploy and reset settings contract')
  .action(async function() {
    await updateSettingsAction()
  })
  program
  .command('updateScheduler')
  .description('Deploy and refresh scheduler: stop, updateops, start')
  .action(async function() {
    await updateSchedulerAction()
  })

  program
  .command('createTestTokens')
  .description('createTestTokens')
  .action(async function() {
    createTestToken()
  })

program
  .command('list')
  .description('List all contracts / accounts')
  .action(async function() {
    console.print("\nSeeds Bank Accounts\n")
    allBankAccountNames.forEach(item=>console.print(item))
    console.print("\nSeeds Contracts\n")
    allContractNames.forEach(item=>console.print(item))
  })

program
  .command('changekey <contract> <key>')
  .description('Change owner and active key')
  .action(async function(contract, key) {
    console.print(`Change key of ${contract} to `+key + "\n")
    await changeOwnerAndActivePermission(contract, key)
  })

program
  .command('change_key_permission <contract> <role> <parentrole> <key>')
  .description('Change owner and active key')
  .action(async function(contract, role, parentrole, key) {
    console.print(`Change key of ${contract} to `+key + "\n")
    await changeExistingKeyPermission(contract, role, parentrole, key)
  })

program
  .command('docsgen <contract> [moreContracts...]')
  .description('Exports SDK docs for contract')
  .action(async function(contract, moreContracts) {
    await batchCallFunc(contract, moreContracts, docsgen)
  })

program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}