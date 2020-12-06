const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, guardians, harvest, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser } = names


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

describe('guardians', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, guardians })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset guardians')
  await contracts.guardians.reset({ authorization: `${guardians}@active` })

  console.log('add users')
  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'Second user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'Third user', "individual", { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, '4th user', "individual", { authorization: `${accounts}@active` })

  await addActorPermission(firstuser, "owner", guardians, "eosio.code");

  console.log('init guardians')
  await contracts.guardians.init(firstuser, [seconduser, thirduser, fourthuser], { authorization: `${firstuser}@active` })
  
  const keyPair = await createKeypair()
  console.log("new account keys: "+JSON.stringify(keyPair, null, 2))

  console.log('init recovery')
  await contracts.guardians.recover(seconduser, firstuser, keyPair.public, { authorization: `${seconduser}@active` })
  
  const recovers = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })
  console.log("recovers: "+JSON.stringify(recovers, null, 2))

  console.log('2nd recovery')
  await contracts.guardians.recover(thirduser, firstuser, keyPair.public, { authorization: `${thirduser}@active` })

  const recovers2 = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })
  console.log("recovers2: "+JSON.stringify(recovers2, null, 2))

  const perms = await eos.getAccount(firstuser)

  console.log("perms: "+JSON.stringify(perms.permissions, null, 2))

  console.log('3nd recovery')
  await contracts.guardians.recover(fourthuser, firstuser, keyPair.public, { authorization: `${fourthuser}@active` })

  const recovers3 = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })
  console.log("recovers3: "+JSON.stringify(recovers3, null, 2))

  console.log('cancel guardians')
  try {
    await contracts.guardians.cancel(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    //console.log("expected error "+ err)
  }

  console.log('reset back to normal dev key')

  const dev_pubkey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
  await contracts.guardians.recover(seconduser, firstuser, dev_pubkey, { authorization: `${seconduser}@active` })
  await contracts.guardians.recover(fourthuser, firstuser, dev_pubkey, { authorization: `${fourthuser}@active` })

  const guards = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'guards',
    json: true
  })
  console.log("guards: "+JSON.stringify(guards, null, 2))


  await contracts.guardians.cancel(firstuser, { authorization: `${firstuser}@active` })

  const guardsAfter = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'guards',
    json: true
  })
  console.log("guardsAfter: "+JSON.stringify(guardsAfter, null, 2))

})
