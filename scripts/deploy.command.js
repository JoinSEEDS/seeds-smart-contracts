const fs = require('fs')
const path = require('path')

const { eos, encodeName, accounts, ownerPublicKey, activePublicKey } = require('./helper')

const deploy = async (name) => {
    const { code, abi } = await source(name)

    let account = accounts[name]
    console.log(`Look ${account.name}`)
    let contractName = account.name

    await createAccount(account)

    console.log("createAccount ", JSON.stringify(account.account));

    if (!code)
      throw new Error('code not found')

    if (!abi)
      throw new Error('abi not found')

    await eos.setcode({
      account: account.account,
      code,
      vmtype: 0,
      vmversion: 0
    }, {
      authorization: `${account.account}@owner`
    })

    await eos.setabi({
      account: account.account,
      abi: JSON.parse(abi)
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
    console.log(`Look ${account}`)
    await eos.transaction(async trx => {
      await trx.newaccount({
        creator,
        name: account,
        owner: publicKey,
        active: publicKey
      })

      await trx.buyrambytes({
        payer: creator,
        receiver: account,
        bytes: stakes.ram
      })

      await trx.delegatebw({
        from: creator,
        receiver: account,
        stake_net_quantity: stakes.net,
        stake_cpu_quantity: stakes.cpu,
        transfer: 0
      })
    })
    console.log(`${account} created`)
  } catch (err) {
    console.error(`account ${account} already created`, err)
  }
}


module.exports = deploy
