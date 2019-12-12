const fs = require('fs')
const { eos, names } = require('./helper')

const { accounts, token } = names

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

  console.log("BACKING UP users for "+accounts + " and " + token)

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
    limit: 300
  })

  fs.writeFileSync('users_backup_'+accounts+'.json', JSON.stringify(users, null, 2))

  console.log('users: '+JSON.stringify(users, null,2))

  let user_accounts = users.rows.map(({ account }) => account )

  console.log('accounts: '+JSON.stringify(user_accounts, null,2))

  console.log("saved "+user_accounts.length + " users")

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



}

main()
