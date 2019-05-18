const fs = require('fs')
const path = require('path')
const { eos, accounts } = require('./helper')

const source = async (name) => {
  const codePath = path.join(__dirname, '..', name.concat('.wasm'))
  const abiPath = path.join(__dirname, '..', name.concat('.abi'))

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
    console.error(`failed create ${account} by ${creator}`, err)
  }
}

const deploy = async ({ name, account }) => {
  try {
    const { code, abi } = await source(name)

    if (!code)
      throw new Error('code not found')

    if (!abi)
      throw new Error('abi not found')

    await eos.setcode({
      account,
      code,
      vmtype: 0,
      vmversion: 0
    }, {
      authorization: `${account}@owner`
    })

    await eos.setabi({
      account,
      abi: JSON.parse(abi)
    }, {
      authorization: `${account}@owner`
    })
    console.log(`${name} deployed to ${account}`)
  } catch (err) {
   console.error(`failed deploy ${name} to ${account}`, err)
  }
}

const setCodePermission = async ({ account, publicKey }) => {
  try {
    await eos.updateauth({
      account,
      auth: {
        [account]: [{
            permission: {
              actor: account,
              permission: 'eosio.code'
            },
            weight: 1
        }],
        keys: [{
          key: publicKey,
          weight: 1
        }],
        threshold: 1,
        waits: []
      },
      parent: "",
      permission: "owner"
    }, { authorization: `${account}@owner` })
    console.log(`${account} permission updated`)
  } catch (err) {
    console.error(`failed permission update to ${account} by ${creator}`, err)
  }
}

const createToken = async ({ account, issuer, supply }) => {
  try {
    const token = await eos.contract(account)
    await token.create({
      issuer,
      maximum_supply: supply
    }, {
      authorization: `${account}@active`
    })
    console.log(`token created to ${issuer}`)
  } catch (err) {
    console.error(`failed to create token on ${account} to ${issuer}`, err)
  }
}

const issueToken = async ({ account, issuer, supply }) => {
  try {
    const token = await eos.contract(account)
    await token.issue({
      memo: '',
      quantity: supply,
      to: issuer
    }, {
      authorization: `${issuer}@active`
    })
    console.log(`token issued to ${issuer}`)
  } catch (err) {
    console.error(`failed to issue tokens on ${account} to ${issuer}`, err)
  }
}

(async () => {
  // NOTE: ensure existing CREATOR account before running deployment script

  await createAccount(accounts.firstuser)
  await createAccount(accounts.seconduser)
  await createAccount(accounts.application)
  await createAccount(accounts.bank)

  await createAccount(accounts.token)
  await deploy(accounts.token)
  await createToken(accounts.token)
  await issueToken(accounts.token)

  await createAccount(accounts.accounts)
  await deploy(accounts.accounts)
  await setCodePermission(accounts.accounts)

  await createAccount(accounts.harvest)
  await deploy(accounts.harvest)

  await createAccount(accounts.subscription)
  await deploy(accounts.subscription)
})()
