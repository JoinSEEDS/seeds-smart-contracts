const fs = require('fs')
const path = require('path')
const R = require('ramda')
const { encodeName, accounts, ownerPublicKey, activePublicKey } = require('./helper')

const debug = process.env.DEBUG || false

console.log = ((showMessage) => {
  return (msg) => {
    showMessage('+ ', msg)
  }
})(console.log)

console.error = ((showMessage) => {
  return (msg, err) => {
    if (process.env.DEBUG === 'true' && err) {
      showMessage('- ', msg, err)
    } else {
      showMessage('- ', msg)
    }
  }
})(console.error)

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
    console.error(`account ${name} already deployed`, err)
  }
}

const changeKeyPermission = async (account, permission, key) => {
  try {
    const { permissions } = await eos.getAccount(account)

    const { parent, required_auth } = permissions.find(p => p.perm_name === permission)

    const { threshold, waits, keys, accounts } = required_auth

    const newPermissions = {
      account,
      permission,
      parent,
      auth: {
        threshold,
        waits,
        accounts,
        keys: [{ key, weight: 1 }]
      }
    }

    await eos.updateauth(newPermissions, { authorization: `${account}@owner` })
    console.log(`private keys updated for ${account}@${permission}`)
  } catch (err) {
    console.error(`failed keys update for ${account}@${permission}`, err)
  }
}

const addPermission = async ({ account: target }, targetRole, { account: actor }, actorRole) => {
  try {
    const { parent, required_auth: { threshold, waits, keys, accounts } } =
      (await eos.getAccount(target))
        .permissions.find(p => p.perm_name == targetRole)

    const existingPermission = accounts.find(({ permission }) =>
      permission.actor == actor && permission.permission == actorRole
    )

    if (existingPermission)
      return console.error(`permission ${actor}@${actorRole} already exists for ${target}@${targetRole}`)

    const permissions = {
      account: target,
      permission: targetRole,
      parent,
      auth: {
        threshold,
        waits,
        accounts: [
          ...accounts,
          {
            permission: {
              actor,
              permission: actorRole
            },
            weight: 1
          }
        ],
        keys: [
          ...keys
        ]
      }
    }

    await eos.updateauth(permissions, { authorization: `${target}@owner` })
    console.log(`permission created on ${target}@${targetRole} for ${actor}@${actorRole}`)
  } catch (err) {
    console.error(`failed permission update on ${target} for ${actor}`, err)
  }
}

const createAccounts = (accounts) => Promise.all(
  accounts.map(createAccount)
)

const deployContracts = (contracts) => Promise.all(
  contracts.map(deploy)
)

const addPermissions = (permissions) => Promise.all(
  permissions.map(
    permission => addPermission(...permission)
  )
)

const configure = ({ account }) => (params) =>
  eos.contract(account)
    .then(contract => Promise.all(
      Object.keys(params)
        .map(param =>
          contract.configure(param, params[param], { authorization: `${account}@active` })
            .then(() => console.log(`configure ${param} equal to ${params[param]}`))
            .catch(err => console.error(`cannot configure ${param}`, err))
        )
    ))

const addCoins = ({ account, issuer, supply }) => (accounts) =>
  eos.contract(account)
    .then(contract =>
      contract.create({
        issuer: issuer,
        maximum_supply: supply
      }, { authorization: `${account}@active` })
        .then(() => {
          console.log(`token created to ${issuer}`)
          return contract.issue({
            to: issuer,
            quantity: supply,
            memo: ''
          }, { authorization: `${issuer}@active` })
            .then(() => {
              console.log(`token issued to ${issuer} (${supply})`)
              return Promise.all(
                accounts.map(({ account, quantity }) =>
                  contract.transfer({
                    from: issuer,
                    to: account,
                    quantity,
                    memo: ''
                  }, { authorization: `${issuer}@active` })
                    .then(() => console.log(`token sent to ${user} (${quantity})`))
                    .catch((err) => console.error(`failed to transfer from ${issuer} to ${user} on ${account}`, err))
                )
              )
            })
            .catch(() => console.log(`failed to issue tokens on ${account} to ${issuer}`))
        })
        .catch((err) => console.error(`token ${account} already created`, err))
    )

const joinUsers = ({ account }) => (users) =>
  eos.contract(account)
    .then(contract => Promise.all(
      users.map(user =>
        contract.adduser(user.account, { authorization: `${account}@active` })
          .then(() => console.log(`joined user ${user.account} on ${account}`))
          .catch((err) => console.error(`user ${user.account} already joined`, err))
      )
    ))

const codeOwnerPermission = (account) =>
  [account, 'owner', account, 'eosio.code']

const codeActivePermission = (account) =>
  [account, 'active', account, 'eosio.code']

const accountActivePermission = (subject, object) =>
  [subject, 'active', object, 'active']

const updatePrivateKeys = async () => {
  const targets = Object.keys(accounts).map(key => accounts[key].account)

  for (let i = 0; i < targets.length; i++) {
    if (targets[i] !== 'seedssubs222') {
      await changeKeyPermission(targets[i], 'active', activePublicKey)
      await changeKeyPermission(targets[i], 'owner', ownerPublicKey)
    }
  }
}

const init = async () => {
  const {
    owner, firstuser, seconduser, thirduser, application, bank,
    token, harvest, subscription, settings, proposals, policy,
    accounts: accts
  } = accounts

  try {
    await eos.getAccount(owner.account)
  } catch (err) {
    console.error(`Please, deploy owner account manually before running script`, err)
    return
  }

  await createAccount(firstuser)
  await createAccount(seconduser)
  await createAccount(application)
  await createAccount(bank)
  await createAccount(accts)
  await createAccount(harvest)
  await createAccount(subscription)
  await createAccount(token)
  await createAccount(settings)
  await createAccount(proposals)
  await createAccount(policy)

  await deploy(token)
  await deploy(accts)
  await deploy(harvest)
  await deploy(subscription)
  await deploy(settings)
  await deploy(proposals)
  await deploy(policy)

  await addPermission(accts, 'owner', accts, 'eosio.code')
  await addPermission(harvest, 'active', harvest, 'eosio.code')
  await addPermission(subscription, 'active', subscription, 'eosio.code')
  await addPermission(proposals, 'active', proposals, 'eosio.code')
  await addPermission(bank, 'active', harvest, 'active')
  await addPermission(bank, 'active', subscription, 'active')
  await addPermission(bank, 'active', proposals, 'active')
  await addPermission(settings, 'active', accts, 'eosio.code')

  await addCoins(token)([ firstuser, seconduser, bank ])

  await configure(settings)({
    tokenaccnt: encodeName(token.account, false),
    bankaccnt: encodeName(bank.account, false),
    hrvstreward: 100000,
    propminstake: 100
  })
}

init()