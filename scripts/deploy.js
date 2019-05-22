const fs = require('fs')
const path = require('path')
const { eos, accounts } = require('./helper')

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
    console.log(`token issued to ${issuer} (${supply})`)
  } catch (err) {
    console.error(`failed to issue tokens on ${account} to ${issuer}`, err)
  }
}

const transfer = async ({ account, issuer }, { account: user }, quantity) => {
  try {
    const token = await eos.contract(account)
    await token.transfer({
      from: issuer,
      to: user,
      quantity,
      memo: ''
    }, {
      authorization: `${issuer}@active`
    })
    console.log(`token sent to ${user} (${quantity})`)
  } catch (err) {
    console.log(`failed to transfer from ${issuer} to ${user} on ${account}`)
  }
}

(async () => {
  // NOTE: ensure existing CREATOR account before running deployment script

  const {
    firstuser, seconduser, application, bank,
    token, harvest, subscription, accounts: accts
  } = accounts

  await createAccount(firstuser)
  await createAccount(seconduser)
  await createAccount(application)
  await createAccount(bank)
  await createAccount(accts)
  await createAccount(harvest)
  await createAccount(subscription)
  await createAccount(token)

  await deploy(token)
  await deploy(accts)
  await deploy(harvest)
  await deploy(subscription)

  await createToken(token)
  await issueToken(token)
  await transfer(token, firstuser, '100000.0000 SEEDS')
  await transfer(token, seconduser, '100000.0000 SEEDS')

  await addPermission(accts, 'owner', accts, 'eosio.code')
  await addPermission(harvest, 'active', harvest, 'eosio.code')
  await addPermission(subscription, 'active', subscription, 'eosio.code')
  await addPermission(bank, 'active', harvest, 'active')
  await addPermission(bank, 'active', subscription, 'active')
})()
