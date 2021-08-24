const fs = require('fs')
const { promisify } = require('util')
const program = require('commander')

const writeFile = promisify(fs.writeFile)

const accountsFetchData = async () => {
  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
    limit: 100
  })
  
  return { users }
}

const harvestFetchData = async () => {
  const balances = await eos.getTableRows({
    code: harvest,
    scope: harvest,
    table: 'balances',
    json: true,
    limit: 100
  })
  
  return { balances }
}

const fetchers = {
  'accounts': accountsFetchData,
  'harvest': harvestFetchData
}

const migrators = {
  'accounts': accountsMigrate,
  'harvest': harvestMigrate
}

program
  .command('backup <contract>')
  .description('Backup tables from contract')
  .action(async contract => {
    const data = await fetchers[contract]()
    
    await writeFile(`${contract}_backup.json`, JSON.stringify(data))
  })
  
program
  .command('restore <contract>')
  .description('Restore tables to contract')
  .action(async contract => {
    
  })