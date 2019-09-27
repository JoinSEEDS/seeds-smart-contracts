const fs = require('fs')
const { eos, names } = require('./helper')

const { accounts } = names

const main = async () => {
  const contract = await eos.contract(accounts)

  const data = fs.readFileSync('users_backup.json').toString()

  const users = JSON.parse(data)

  const { rows } = users

  for (let i = 0; i < rows.length; i++) {
    const user = rows[i]

    const args = Object.values(user)

    try {
      const { transaction_id } = await contract.migrate(
        user.account,
        user.status,
        user.type,
        user.nickname,
        user.image,
        user.story,
        user.roles,
        user.skills,
        user.interests,
        user.reputation
      , { authorization: `${accounts}@active` })

      console.log(transaction_id)
    } catch (err) {
      console.log('already exists ?!', err)
    }
  }

  console.log('done')
}

main()
