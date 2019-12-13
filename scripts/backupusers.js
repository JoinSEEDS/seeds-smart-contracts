const fs = require('fs')
const { eos, names } = require('./helper')

const { accounts, token, harvest } = names

const getBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  let num = parseFloat(balance[0])
  if (num == NaN) {
    console.log("nil balance for "+user+": "+JSON.stringify(balance))
  }
  return parseFloat(balance[0])
}

const main = async () => {
  const { owner } = accounts

  console.log("BACKING UP users for "+accounts + " token: " + token + " harvest: "+harvest)

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
    limit: 300
  })

  fs.writeFileSync('users_backup_'+accounts+'.json', JSON.stringify(users, null, 2))

  //console.log('users: '+JSON.stringify(users, null,2))

  let user_accounts = users.rows.map(({ account }) => account )

  //console.log('accounts: '+JSON.stringify(user_accounts, null,2))

  console.log("saved "+user_accounts.length + " users")

  
  // Token Balances 

  let account_balances = []
  
  for (let i=0; i<user_accounts.length; i++) {
    let acct = user_accounts[i]
    let balance = await getBalanceFloat(acct)
    console.log(acct+": "+balance)
    account_balances.push({
      account: acct,
      balance: balance
    })
  }

  console.log('balances: '+JSON.stringify(account_balances, null,2))

  fs.writeFileSync('user_balances_'+token+'.json', JSON.stringify(account_balances, null, 2))

  // Harvest Balances

  let done = false
  let entries = []
  let lastEntry = "0"
  while (!done) {
    const harvest_balances = await eos.getTableRows({
      code: harvest,
      scope: harvest,
      table_key: "account",
      table: 'balances',
      json: true,
      lower_bound: lastEntry,
      upper_bound: -1,
      limit: 200
    })

    entries = entries.concat(harvest_balances.rows)

    //console.log('harvest_balances: '+JSON.stringify(harvest_balances.rows, null,2))
    //console.log('entries: '+JSON.stringify(entries, null,2))
    //console.log("last "+entries[entries.length - 1])

    lastEntry = entries[entries.length - 1].account
    done = harvest_balances.more == false

    if (!done) {
      console.log("read "+harvest_balances.length + " items continue at: "+lastEntry)
    }

    
  }

  console.log("Saving Harvest: "+entries.length + " entries")
  //console.log('harvest_balances: '+JSON.stringify(entries, null,2))
  fs.writeFileSync('user_balances_'+harvest+'.json', JSON.stringify(entries, null, 2))

}

main()
