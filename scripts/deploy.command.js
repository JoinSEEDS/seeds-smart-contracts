const fs = require('fs')
const path = require('path')

const { eos, encodeName, accounts, ownerPublicKey, activePublicKey } = require('./helper')

const deploy = async (name) => {
    const { code, abi } = await source(name)

    let account = accounts[name]
    console.log(`deploy ${account.name}`)
    let contractName = account.name

    await createAccount(account)

    console.log("createAccount ", JSON.stringify(account.account));

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
  
    console.log("abi deployed")

    await eos.setcode({
      account: account.account,
      code,
      vmtype: 0,
      vmversion: 0
    }, {
      authorization: `${account.account}@owner`
    })

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
        actions: [{
          account: 'eosio',
          name: 'newaccount',
          authorization: [{
            actor: creator,
            permission: 'active',
          }],
          data: {
            creator: creator,
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
          },
        },
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
            bytes: 8192,
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
            stake_net_quantity: '2.0000 TLOS',
            stake_cpu_quantity: '2.0000 TLOS',
            transfer: false
          }
        }
      ]
      }, {
        blocksBehind: 3,
        expireSeconds: 30,
      });
    console.log(`${account} created`)
  } catch (err) {
    if ((""+err).indexOf("Account name already exists") != -1) {
      console.error(`account ${account} already created`)
    } else {
      console.error(`create account ${account} error: ` + err)
    }
  }
}


module.exports = deploy
