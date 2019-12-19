const fs = require('fs')
const path = require('path')
const R = require('ramda')
const { eos, encodeName, getBalance, accounts, ownerPublicKey, activePublicKey, apiPublicKey, permissions } = require('./helper')

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

const createAccount = async ({ account, publicKey, stakes, creator } = {}) => {
  if (!account) return

  console.log(`account: ` + `${account}`);
  console.log(`publicKey: ` + `${publicKey}`);
  console.log(`stakes: ` + `${stakes}`);
  console.log(`creator: ` + `${creator}`);

  try {
    await eos.transaction(async trx => {
      await trx.newaccount({
        creator,
        name: account,
        owner: publicKey,
        active: publicKey
      }, { authorization: `${creator}@owner` })

      await trx.buyrambytes({
        payer: creator,
        receiver: account,
        bytes: stakes.ram
      }, { authorization: `${creator}@owner` })

      await trx.delegatebw({
        from: creator,
        receiver: account,
        stake_net_quantity: stakes.net,
        stake_cpu_quantity: stakes.cpu,
        transfer: 0
      }, { authorization: `${creator}@owner` })
    })
    console.log(`${account} created`)
  } catch (err) {
    let errStr = err + ""
    if (errStr.includes("Account name already exists")) {
      console.log(`${account} created already`)
    } else {
      console.error(`create account error for: ${account} \n* error: ` + err + `\n`)
    }
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
    let errStr = "" + err
    if (errStr.includes("contract is already running this version of code")) {
      console.log(`${name} deployed to ${account} already`)
    } else {
      console.error(`error deploying account ${name} \n* error: ` + err + `\n`)
    }
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
    console.error(`failed keys update for ${account}@${permission}\n* error: ` + err + `\n`)
  }
}

const createKeyPermission = async (account, role, parentRole = 'active', key) => {
  try {
    await eos.updateauth({
      account,
      permission: role,
      parent: parentRole,
      auth: {
        threshold: 1,
        waits: [],
        accounts: [],
        keys: [{
          key,
          weight: 1
        }]
      }
    }, { authorization: `${account}@owner` })
    console.log(`permission setup on ${account}@${role}(/${parentRole}) for ${key}`)
  } catch (err) {
    console.error(`failed permission setup\n* error: ` + err + `\n`)
  }
}

const allowAction = async (account, role, action) => {
  try {
    await eos.linkauth({
      account,
      code: account,
      type: action,
      requirement: role
    }, { authorization: `${account}@owner` })
    console.log(`linkauth of ${account}@${action} for ${role}`)
  } catch (err) {
    let errString = `failed allow action\n* error: ` + err + `\n`
    if (errString.includes("Attempting to update required authority, but new requirement is same as old")) {
      console.log(`linkauth of ${account}@${action} for ${role} exists`)
    } else {
      console.error(errString)
    }
  }
}

const addActorPermission = async (target, targetRole, actor, actorRole) => {
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
    console.error(`failed permission update on ${target} for ${actor}\n* error: ` + err + `\n`)
  }
}

const createCoins = async (token) => {
  const { account, issuer, supply } = token

  const contract = await eos.contract(account)

  try {
    await contract.create({
      issuer: issuer,
      initial_supply: supply
    }, { authorization: `${account}@active` })

    await contract.issue({
      to: issuer,
      quantity: supply,
      memo: ''
    }, { authorization: `${issuer}@active` })
    
    console.log(`coins successfully minted at ${account} (${supply})`)
  } catch (err) {
    console.error(`coins already created at ${account}\n* error: ` + err + "\n")
  }
}

const transferCoins = async (token, recipient) => {
  const contract = await eos.contract(token)
  try {
    await contract.transfer({
      from: token.issuer,
      to: recipient.account,
      quantity: recipient.quantity,
      memo: ''
    }, { authorization: `${token.issuer}@active` })
    
    console.log(`sent ${recipient.quantity} from ${token.issuer} to ${recipient.account}`)

    console.log("remaining balance for "+token.issuer +" "+ JSON.stringify(await getBalance(token.issuer), null, 2))


  } catch (err) {
    console.error(`cannot transfer from ${token.issuer} to ${recipient.account} (${recipient.quantity})\n* error: ` + err + `\n`)
  }
}

const reset = async ({ account }) => {
  const contract = await eos.contract(account)
  
  try {
    console.log(`will reset contract ${account}`)
    await contract.reset({ authorization: `${account}@active` })
    console.log(`reset contract ${account}`)
  } catch (err) {
    console.error(`cannot reset contract ${account}\n* error: ` + err + `\n`)

  }
}

const resetByName = async ( contractName ) => {
  try {
    await reset(accounts[contractName])
  } catch (err) {
    console.error(`cannot resetByName contract ${contractName}\n* error: ` + err + `\n`)
  }
}

const isExistingAccount = async (account) => {
  let exists = false

  try {
    await eos.getAccount(account)
    exists = true
  } catch (err) {
    exists = false
  }

  return exists
}

const isActionPermission = permission => permission.action
const isActorPermission = permission => permission.actor && !permission.key
const isKeyPermission = permission => permission.key && !permission.actor

const initContracts = async () => {
  const ownerExists = await isExistingAccount(accounts.owner.account)

  if (!ownerExists) {
    console.log(`owner ${accounts.owner.account} should exist before deployment`)
    return
  }

  await createAccount(accounts.token)
  await deploy(accounts.token)
  await createCoins(accounts.token)

  const accountNames = Object.keys(accounts)
  for (let current = 0; current < accountNames.length; current++) {
    const accountName = accountNames[current]
    const account = accounts[accountName]

    await createAccount(account)

    if (account.type === 'contract') {
      await deploy(account)
    }

    if (account.quantity && Number.parseFloat(account.quantity) > 0) {
      await transferCoins(accounts.token, account)
    }
  }

  for (let current = 0; current < permissions.length; current++) {
    const permission = permissions[current]

    if (isActionPermission(permission)) {
      const { target, action } = permission
      const [ targetAccount, targetRole ] = target.split('@')

      await allowAction(targetAccount, targetRole, action)
    } else if (isActorPermission(permission)) {
      const { target, actor } = permission
      const [ targetAccount, targetRole ] = target.split('@')
      const [ actorAccount, actorRole ] = actor.split('@')

      await addActorPermission(targetAccount, targetRole, actorAccount, actorRole)
    } else if (isKeyPermission(permission)) {
      const { target, parent, key } = permission
      const [ targetAccount, targetRole ] = target.split('@')

      await createKeyPermission(targetAccount, targetRole, parent, key)
    } else {
      console.log(`invalid permission #${current}`)
    }
  }
  
  await reset(accounts.settings)
}

module.exports = { initContracts, resetByName }
