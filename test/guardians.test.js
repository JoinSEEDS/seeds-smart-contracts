const { describe } = require("riteway")
const { eos, encodeName, getBalance, getBalanceFloat, names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")
const { equals } = require("ramda")
const { assert } = require("chai")

const { accounts, guardians } = names

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

describe('guardians', async a => {
  const assert = (fns, message) => {
    fns = typeof fns === 'array' ? fns : [fns]

    return Promise.all(fns).then(() => {
      a({
        given: message,
        should: '[Done]',
        actual: true,
        expected: true
      })
    }).catch((err) => {
      a({
        given: message,
        should: ['[Failed]'],
        actual: false,
        expected: true
      })
    })

  }

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, guardians })

  const {
    firstuser: protectableAccount,
    seconduser: firstGuardian,
    thirduser: secondGuardian,
    fourthuser: thirdGuardian
  } = names

  const claimDelaySec = 1;

  const keyPair = await createKeypair()

  assert(contracts.accounts.reset({ authorization: `${accounts}@active` }),
    'reset accounts')

  assert([
    contracts.accounts.adduser(protectableAccount, 'First user', "individual", { authorization: `${accounts}@active` }),
    contracts.accounts.adduser(firstGuardian, 'Second user', "individual", { authorization: `${accounts}@active` }),
    contracts.accounts.adduser(secondGuardian, 'Third user', "individual", { authorization: `${accounts}@active` }),
    contracts.accounts.adduser(thirdGuardian, '4th user', "individual", { authorization: `${accounts}@active` })  
  ], 'add users')

  assert(addActorPermission(protectableAccount, "owner", guardians, "eosio.code"),
    'add actor permission')

  assert(contracts.guardians.setup(protectableAccount, claimDelaySec, guardianInviteHash, { authorization: `${protectableAccount}@active` }), 
    'setup protectable account')

  assert([
    contracts.guardians.protect(firstGuardian, protectableAccount, guardianInviteSecret, { authorization: `${firstGuardian}@active` }),
    contracts.guardians.protect(secondGuardian, protectableAccount, guardianInviteSecret, { authorization: `${firstGuardian}@active` }),
    contracts.guardians.protect(thirdGuardian, protectableAccount, guardianInviteSecret, { authorization: `${firstGuardian}@active` }) 
  ], 'protect account with three guardians')

  assert(contracts.guardians.activate(protectableAccount, { authorization: `${protectableAccount}@active`}),
    'activate account protection')

  assert(contracts.guardians.pause(protectableAccount, { authorization: `${protectableAccount}@active` }),
    'pause account protection')

  assert(contracts.guardians.unpause(protectableAccount, { authorization: `${protectableAccount}@active` }),
    'unpause account protection')
  
  assert(contracts.guardians.newrecovery(firstGuardian, protectableAccount, keyPair.public, { authorization: `${firstGuardian}@actvie` }),
    'begin recovery process')

  assert(contracts.guardians.norecovery(secondGuardian, protectableAccount), { authorization: `${secondGuardian}@active` },
    'cancel recovery process')

  assert(contracts.guardians.newrecovery(secondGuardian, protectableAccount, keyPair.public, { authorization: `${secondGuardian}@actvie` }),
    'begin recovery process again')

  assert(contracts.guardians.yesrecovery(thirdGuardian, protectableAccount, { authorization: `${thirdGuardian}@active` }),
    'confirm recovery process')

  setTimeout(() => {
    assert(contracts.guardians.claim(protectableAccount, { authorization: `${protectableAccount}@active`}),
      'claim account after delay'    
    )
  }, claimDelaySec * 1000)
})