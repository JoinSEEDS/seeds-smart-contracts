const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")
const { equals } = require("ramda")

const { accounts, guardians, harvest, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser } = names

const dev_pubkey = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"

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
            weight: threshold
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

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
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
  await contracts.guardians.init(firstuser, [seconduser, thirduser, fourthuser], 0, { authorization: `${firstuser}@active` })
  
  const guards = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'guards',
    lower_bound: firstuser,
    upper_bound: firstuser,
    json: true
  })

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

  console.log('2nd recovery')
  await contracts.guardians.recover(thirduser, firstuser, keyPair.public, { authorization: `${thirduser}@active` })

  const recovers2 = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })

  console.log("claim recovery - anyone can claim")
  await contracts.guardians.claim(firstuser, { authorization: `${guardians}@application` })

  const perms = await eos.getAccount(firstuser)

  //console.log("perms: "+JSON.stringify(perms.permissions, null, 2))
  const newAccountActiveKey = perms.permissions[0].required_auth.keys[0].key
  const newAccountOwnerKey = perms.permissions[1].required_auth.keys[0].key

  const recovers3 = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })

  console.log('cancel guardians')
  let oldKeyStillWorks = true
  try {
    await contracts.guardians.cancel(firstuser, { authorization: `${firstuser}@active` })
  } catch (err) {
    //console.log("expected error "+ err)
    oldKeyStillWorks = false
  }

  console.log('reset back to normal dev key')
  await contracts.guardians.recover(seconduser, firstuser, dev_pubkey, { authorization: `${seconduser}@active` })
  await contracts.guardians.recover(fourthuser, firstuser, dev_pubkey, { authorization: `${fourthuser}@active` })
  await contracts.guardians.claim(firstuser, { authorization: `${guardians}@application` })

  console.log('rogue recover')
  await contracts.guardians.recover(seconduser, firstuser, keyPair.public, { authorization: `${seconduser}@active` })
  await contracts.guardians.recover(thirduser, firstuser, keyPair.public, { authorization: `${thirduser}@active` })
  const recoversRogue = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })

  console.log('cancel guardians')
  await contracts.guardians.cancel(firstuser, { authorization: `${firstuser}@active` })

  const guardsAfter = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'guards',
    lower_bound: firstuser,
    upper_bound: firstuser,
    json: true
  })

  const recovers4 = await eos.getTableRows({
    code: guardians,
    scope: guardians,
    table: 'recovers',
    json: true
  })

  assert({
    given: "init guardians",
    should: "have entry in guardians table",
    expected: 1,
    actual: guards.rows.length
  })

  assert({
    given: "recovery was initiated",
    should: "have entry in recovery",
    expected: {
      "account": "seedsuseraaa",
      "guardians": [
        "seedsuserbbb"
      ],
      "public_key": keyPair.public,
      "complete_timestamp": 0
    },
    actual: recovers.rows[0]
  })

  assert({
    given: "recovery complete - 2/3",
    should: "have 2 signatures",
    expected: 2,
    actual: recovers2.rows[0].guardians.length
  })

  assert({
    given: "recovery complete - marked by timestamp",
    should: "have timestamp",
    expected: true,
    actual: recovers2.rows[0].complete_timestamp > 0
  })

  assert({
    given: "recovery claimed",
    should: "have new owner key",
    expected: keyPair.public,
    actual: newAccountOwnerKey
  })

  assert({
    given: "recovery claimed active key",
    should: "have new active key",
    expected: keyPair.public,
    actual: newAccountActiveKey
  })

  assert({
    given: "recovery claimed - recovery table empty",
    should: "no more recovery entries",
    expected: [],
    actual: recovers3.rows
  })

  assert({
    given: "new key was set",
    should: "old key can't be used to sign transactions",
    expected: false,
    actual: oldKeyStillWorks
  })


  assert({
    given: "guardians were cancelled",
    should: "have no more guardians",
    expected: [],
    actual: guardsAfter.rows
  })
  assert({
    given: "guardians were cancelled",
    should: "have no more recovers",
    expected: [],
    actual: recovers4.rows
  })
})

describe('Time lock safety', async assert => {

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
  await contracts.guardians.init(firstuser, [seconduser, thirduser, fourthuser], 5, { authorization: `${firstuser}@active` })
  
  console.log('rogue recovery')
  await contracts.guardians.recover(seconduser, firstuser, dev_pubkey, { authorization: `${seconduser}@active` })
  await contracts.guardians.recover(thirduser, firstuser, dev_pubkey, { authorization: `${thirduser}@active` })

  console.log("claim recovery")
  var canClaimBeforeTimelock = true
  var errString = ""
  try {
    await contracts.guardians.claim(firstuser, { authorization: `${guardians}@application` })
  } catch (err) {
    canClaimBeforeTimelock = false
    //console.log("expected error: "+err)
    errString = ""+err
  }

  await sleep(5000)
  await contracts.guardians.claim(firstuser, { authorization: `${guardians}@application` })

  assert({
    given: "claim before time lock",
    should: "fails",
    expected: false,
    actual: canClaimBeforeTimelock
  })

  assert({
    given: "claim before time lock",
    should: "have error string",
    expected: true,
    actual: errString.toLowerCase().indexOf("need to wait another") != -1
  })

})
