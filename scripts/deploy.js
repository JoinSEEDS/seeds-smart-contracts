const fs = require('fs')
const path = require('path')
const R = require('ramda')
const { eos, isLocal, encodeName, getBalance, accounts, ownerPublicKey, activePublicKey, apiPublicKey, permissions, sleep } = require('./helper')

const debug = process.env.DEBUG || false

console.print = ((showMessage) => {
  return (msg) => {
    showMessage(msg)
  }
})(console.log)

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
      console.error("unknown delegatebw action")
    }

    console.log(`${account} created`)
  } catch (err) {
    let errStr = err + ""
    if (errStr.includes("as that name is already taken")) {
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
      authorization: `${account}@active`
    })

    await eos.setabi({
      account,
      abi: JSON.parse(abi)
    }, {
      authorization: `${account}@active`
    })
    console.log(`${name} deployed to ${account}`)
  } catch (err) {
    let errStr = "" + err
    if (errStr.toLowerCase().includes("contract is already running this version")) {
      console.log(`${name} deployed to ${account} already`)
    } else {
      console.error(`error deploying account ${name} \n* error: ` + err + `\n`)
    }
  }
}

const createKeyPermission = async (account, role, parentRole = 'active', key) => {
  try {
    const { permissions } = await eos.getAccount(account)

    const perm = permissions.find(p => p.perm_name === role)

    if (perm) {
      const { parent, required_auth } = perm
      const { keys } = required_auth
  
      if (keys.find(item => item.key === key)) {
        console.log("- createKeyPermission key already exists "+key)
        return;
      }  
    }

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

const createActorPermission = async (account, role, parentRole = 'active', actor, actorRole='eosio.code') => {
  try {
    const { permissions } = await eos.getAccount(account)

    const perm = permissions.find(p => p.perm_name === role)

    if (perm) {
      const { parent, required_auth } = perm
      const { accounts } = required_auth
  
      if (accounts.find(item => item.actor === actor && item.permission == actorRole)) {
        console.log("- createActorPermission already exists "+actor+"@"+actorRole)
        return;
      }  
    }

    await eos.updateauth({
      account,
      permission: role,
      parent: parentRole,
      auth: {
        threshold: 1,
        waits: [],
        accounts: [
          {
            permission: {
              actor,
              permission: actorRole
            },
            weight: 1
          }
        ],
        keys: []
      }
    }, { authorization: `${account}@owner` })
    console.log(`permission setup on ${account}@${role}(/${parentRole}) for ${actor+"@"+actorRole}`)
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
      console.log(`- linkauth of ${account}@${action} for ${role} exists`)
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
      return console.error(`- permission ${actor}@${actorRole} already exists for ${target}@${targetRole}`)

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
    console.log(`+ permission created on ${target}@${targetRole} for ${actor}@${actorRole}`)
  } catch (err) {
    console.error(`failed permission update on ${target} for ${actor}\n* error: ` + err + `\n`)
  }
}

const removeAllActorPermissions = async (target) => {
  await removeActorPermission(target, "active")
  await removeActorPermission(target, "owner")
}

const removeActorPermission = async (target, targetRole) => {
  console.log("remove "+target + "@" + targetRole)
  try {
    const { parent, required_auth: { threshold, waits, keys, accounts } } =
      (await eos.getAccount(target))
        .permissions.find(p => p.perm_name == targetRole)

    const permissions = {
      account: target,
      permission: targetRole,
      parent,
      auth: {
        threshold,
        waits,
        accounts: [],
        keys: [
          ...keys
        ]
      }
    }

    await eos.updateauth(permissions, { authorization: `${target}@owner` })
    console.log(`+ actor permissions removed on ${target}@${targetRole}`)
  } catch (err) {
    console.error(`failed permission update on ${target}\n* error: ` + err + `\n`)
  }
}

const changeOwnerAndActivePermission = async (account, key) => {
  await changeExistingKeyPermission(account, "active", "owner", key)
  await changeExistingKeyPermission(account, "execute", "active", key)
  await changeExistingKeyPermission(account, "owner", "", key)
}

const changeExistingKeyPermission = async (account, role, parentRole = 'active', key) => {
  try {
    const { permissions } = await eos.getAccount(account)

    //console.log(account + " permissions: "+ JSON.stringify(permissions, null, 2))

    const perm = permissions.find(p => p.perm_name === role)

    if (!perm) {
      console.error(`permission not found : ` + role + `\n`)
      return
    }
    const { required_auth } = perm
    const { keys, accounts, waits } = required_auth
  
    if (keys.find(item => item.key === key)) {
      console.log("- createKeyPermission key already exists "+key)
      return;
    }  
    
    await eos.updateauth({
      account,
      permission: role,
      parent: parentRole,
      auth: {
        threshold: 1,
        waits: waits,
        accounts: accounts,
        keys: [{
          key,
          weight: 1
        }]
      }
    }, { authorization: `${account}@owner` })

    //const afterAcct = await eos.getAccount(account)
    //console.log(account + "=== AFTER permissions: "+ JSON.stringify(afterAcct.permissions, null, 2))

    console.log(`permission setup on ${account}@${role}(/${parentRole}) for ${key}`)
  } catch (err) {
    console.error(`failed permission setup\n* error: ` + err + `\n`)
  }
}

const createCoins = async (token) => {
  const { account, issuer, supply } = token

  try {
    await eos.transaction({
      actions: [
        {
          account,
          name: 'create',
          authorization: [{
            actor: account,
            permission: 'active'
          }],
          data: {
            issuer,
            initial_supply: supply
          }
        }
      ]
    })
  
    await eos.transaction({
      actions: [
        {
          account,
          name: 'issue',
          authorization: [{
            actor: issuer,
            permission: 'active'
          }],
          data: {
            to: issuer,
            quantity: supply,
            memo: ''
          }
        }
      ]
    })
    console.log(`coins successfully minted at ${account} with max supply of ${supply}`)
  } catch (err) {
    console.error(`coins already created at ${account}\n* error: ` + err + "\n")
  }
}

const transferCoins = async (token, recipient) => {
  try {

    await eos.transaction({
      actions: [
        {
          account: token.account,
          name: 'transfer',
          authorization: [{
            actor: token.issuer,
            permission: 'active'
          }],
          data: {
            from: token.issuer,
            to: recipient.account,
            quantity: recipient.quantity,
            memo: ''
          }
        }
      ]
    })
    
    console.log(`sent ${recipient.quantity} from ${token.issuer} to ${recipient.account}`)

    console.log("remaining balance for "+token.issuer +" "+ JSON.stringify(await getBalance(token.issuer), null, 2))

  } catch (err) {
    console.error(`cannot transfer from ${token.issuer} to ${recipient.account} (${recipient.quantity})\n* error: ` + err + `\n`)
  }
}

const reset = async ({ account }) => {

  if (!isLocal()) {
    console.log("Don't reset contracts on testnet or mainnet!")
    return
  }

  try {
    console.log(`will reset contract ${account}`)

    await eos.transaction({
      actions: [
        {
          account,
          name: 'reset',
          authorization: [{
            actor: account,
            permission: 'active'
          }],
          data: {}
        }
      ]
    })
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
const isCreateActorPermission = permission => permission.type == "createActorPermission"
const isKeyPermission = permission => permission.key && !permission.actor

const updatePermissions = async () => {

  console.log("Updating permissions...")

  for (let current = 0; current < permissions.length; current++) {
    const permission = permissions[current]

    if (isActionPermission(permission)) {
      const { target, action } = permission
      const [ targetAccount, targetRole ] = target.split('@')
      await allowAction(targetAccount, targetRole, action)

    } else if (isCreateActorPermission(permission)) {
      const { target, actor, parent } = permission
      const [ targetAccount, targetRole ] = target.split('@')
      const [ actorAccount, actorRole ] = actor.split('@')
      await createActorPermission(targetAccount, targetRole, parent, actorAccount, actorRole)

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
}

const createTestToken = async () => {
  await createCoins(accounts.testtoken)
}

const deployAllContracts = async () => {
  const ownerExists = await isExistingAccount(accounts.owner.account)

  if (!ownerExists) {
    console.log(`owner ${accounts.owner.account} should exist before deployment`)
    return
  }

  await createAccount(accounts.token)
  await deploy(accounts.token)
  await createCoins(accounts.token)

  if (accounts.testtoken) {
    await createCoins(accounts.testtoken)
  }

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

    await sleep(1000)
  }

  if (isLocal()) {
    await addActorPermission("cg.seeds", "active", "seedsuseraaa", "active")
    await addActorPermission("cg.seeds", "active", "seedsuserbbb", "active")
  }
  
  await updatePermissions()
  await reset(accounts.settings)
}

module.exports = { 
  source, deployAllContracts, updatePermissions, 
  resetByName, changeOwnerAndActivePermission, 
  changeExistingKeyPermission, addActorPermission, createTestToken,
  removeAllActorPermissions
}
