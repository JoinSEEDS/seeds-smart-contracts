const fs = require('fs')
const path = require('path')

const { eos, encodeName, accounts, ownerPublicKey, activePublicKey } = require('./helper')

const deploy = async (name) => {
    const { code, abi } = await source(name)

    let account = accounts[name]
    console.log(`deploy ${account.name}`)
    let contractName = account.name
    
    await createAccount(account)

    if (!code)
      throw new Error('code not found')

    if (!abi)
      throw new Error('abi not found')

    await eos.setabi({
        account: account.account,
        abi: JSON.parse(abi)
      }, {
        authorization: `${account.account}@owner`
      })
  
    await eos.setcode({
      account: account.account,
      code,
      vmtype: 0,
      vmversion: 0
    }, {
      authorization: `${account.account}@owner`
    })
    console.log("code deployed")


  console.log("abi deployed")

  console.log(`Success: ${name} deployed to ${account.account}`)
}

const source = async (name) => {
  const codePath = path.join(__dirname, '../artifacts', name.concat('.wasm'))
  const abiPath = path.join(__dirname, '../artifacts', name.concat('.abi'))

  const code = new Promise(resolve => {
    fs.readFile(codePath, (_, r) => resolve(r))
  })
  const abi = new Promise(resolve => {
    fs.readFile(abiPath, (_, r) => resolve(r))
  })

  return Promise.all([code, abi]).then(([code, abi]) => ({ code, abi }))
}

const createAccount = async ({ account, publicKey, stakes, creator }) => {
  try {

    await eos.transaction({
      actions: [
        {
          account: 'eosio',
          name: 'newaccount',
          authorization: [{
            actor: creator,
            permission: 'active',
          }],
          data: {
            creator,
            name: account,
            owner: {
              threshold: 1,
              keys: [{
                key: publicKey,
                weight: 1
              }],
              accounts: [],
              waits: []
            },
            active: {
              threshold: 1,
              keys: [{
                key: publicKey,
                weight: 1
              }],
              accounts: [],
              waits: []
            },
          }
        }
      ]
    })

    try {
      await eos.transaction({
        actions: [
          {
            account: 'eosio',
            name: 'buyrambytes',
            authorization: [{
              actor: creator,
              permission: 'active',
            }],
            data: {
              payer: creator,
              receiver: account,
              bytes: stakes.ram,
            },
          },
          {
            account: 'eosio',
            name: 'delegatebw',
            authorization: [{
              actor: creator,
              permission: 'active',
            }],
            data: {
              from: creator,
              receiver: account,
              stake_net_quantity: stakes.net,
              stake_cpu_quantity: stakes.cpu,
              transfer: false,
            }
          }
        ]
      })
    } catch (error) {
      console.error("unknown delegatebw action "+error)
    }

    console.log(`${account} created`)
  } catch (err) {
    if ((""+err).indexOf("as that name is already taken") != -1) {
      console.error(`account ${account} already created`)
    } else {
      console.error(`account ${account} create error` + err)
      throw err
    }
  }
}


module.exports = deploy
