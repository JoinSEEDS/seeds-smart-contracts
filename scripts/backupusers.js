const fs = require('fs')
const { eos, names } = require('./helper')

const { accounts } = names

const main = async () => {
  const { owner } = accounts

  const users = await eos.getTableRows({
    code: accounts,
    scope: accounts,
    table: 'users',
    json: true,
    limit: 100
  })

  fs.writeFileSync('users_backup.json', JSON.stringify(users))

  console.log('done')
}

main()
