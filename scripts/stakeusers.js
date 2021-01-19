const fs = require('fs')
const { eos, names } = require('./helper')

const { accounts } = names

const main = async () => {
  const contract = await eos.contract('token.seeds')

  const data = fs.readFileSync('users_backup.json').toString()

  const users = JSON.parse(data)

  const { rows } = users

  for (let i = 0; i < rows.length; i++) {
    const user = rows[i]

    const args = Object.values(user)

    try {

      const { transaction_id } = await eos.transaction({
        actions: [
          {
            account: 'eosio',
            name: 'delegatebw',
            authorization: [{
              actor: 'seedsharvest',
              permission: 'owner',
            }],
            data: {
              from: 'seedsharvest',
              receiver: user.account,
              stake_net_quantity: '0.1000 TLOS',
              stake_cpu_quantity: '2.0000 TLOS',
              transfer: false,
            }
          }
        ]
      })

      // const { transaction_id } = await eos.transaction(async trx => {
      //   await trx.delegatebw({
      //     from: 'seedsharvest',
      //     receiver: user.account,
      //     stake_net_quantity: '0.1000 TLOS',
      //     stake_cpu_quantity: '2.0000 TLOS',
      //     transfer: 0
      //   }, { authorization: `seedsharvest@owner` })
      // })
      
      console.log(transaction_id)
    } catch (err) {
      console.log('already exists ?!', err)
    }
  }

  console.log('done')
}

main()
