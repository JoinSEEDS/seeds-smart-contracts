const assert = require('assert')
const zlib = require('zlib')
const fetch = require('node-fetch')

const eosjs = require('eosjs')
const { Serialize } = eosjs
const { SigningRequest } = require('eosio-signing-request')
const { source } = require('./deploy')
const { accounts, eos } = require('./helper')
const { proposeMsig } = require('./msig')

const authPlaceholder = "............1"
const GuardianAccountName = "cg.seeds"

/**
 * Propose a change in guardians
 * @param {*} proposerAccount proposer - needs to be one of the guardians
 * @param {*} proposalName any
 * @param {*} targetAccount account to change permissions on - should be GuardianAccountName
 * @param {*} guardians a list of accounts that are the new guardians
 */
const proposeChangeGuardians = async (proposerAccount, proposalName, targetAccount, guardians) => {
    console.log('proposeChangeGuardians by '+proposerAccount + " prop name: "+proposalName + " target: "+targetAccount)

    console.log("new proposed guardians")
    console.log(guardians)

    assert(guardians.length > 1, "more than 1 guardian required")
    assert(targetAccount == GuardianAccountName, "target should be " + GuardianAccountName)

    const actions = [
      createPermissionsAction("active", guardians),
      createPermissionsAction("owner", guardians) 
    ]

    const res = await createMultisigProposal(proposerAccount, proposalName, actions, "owner")

    const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

    console.log("ESR for Approve: " + JSON.stringify(approveESR))
  
    const execESR = await createESRCodeExec({proposerAccount, proposalName})
  
    console.log("ESR for Exec: " + JSON.stringify(execESR))
    
}

const createPermissionsAction = (permission, guardians) => {

  const targetAccount = GuardianAccountName

  assert(permission == "active" || permission == "owner", "permission must be active or owner")
  const parent = permission == 'owner' ? "." : "owner"

  var accountPermissions = guardians.map(item => ({ 
    permission: { 
      actor: item, 
      permission: permission
    },
    weight: 1
  }))

  const threshold = accountPermissions.length > 2 ? accountPermissions.length - 2 : 2

  console.log("creating " + threshold + " out of "+accountPermissions.length + " permissions for @"+ permission)

  accountPermissions.push({
    permission: { 
      actor: "msig.seeds", 
      permission: "eosio.code"
    },
    weight: threshold
  })

  accountPermissions.sort((a, b) => {
    if (a.permission.actor <= b.permission.actor) {
      return -1
    }
    return 1
  })

  const auth = {
    threshold,
    waits: [],
    accounts: accountPermissions,
    keys: []
  }

  const action = {
      account: 'eosio',
      name: 'updateauth',
      authorization: [{
        actor: targetAccount,
        permission: "owner",
      }],
      data: {
        account: targetAccount,
        permission: permission,
        parent,
        auth
      },
  }

  console.log("action to be proposed: "+JSON.stringify(action, null, 2))

  return action
}

/**
 * Revert account back to key permission
 * @param {*} proposerAccount 
 * @param {*} proposalName 
 * @param {*} targetAccount 
 * @param {*} permission_name 
 */
const proposeKeyPermissions = async (proposerAccount, proposalName, targetAccount, permission_name, public_key) => {
    console.log('proposeKeyPermissions '+targetAccount + " with key "+public_key)
  
    assert(permission_name == "active" || permission_name == "owner", "permission must be active or owner")

    const parent = permission_name == 'owner' ? "." : "owner"
    const { permissions } = await eos.getAccount(targetAccount)
    const perm = permissions.find(p => p.perm_name === permission_name)
    console.log("existing permission "+JSON.stringify(perm, null, 2))
    const { required_auth } = perm
    const { keys, accounts, waits } = required_auth
    const auth = {
        threshold: 1,
        waits: [],
        accounts: [],
        keys: [{
          key: public_key,
          weight: 1
        }]
      }  
    console.log("new permissions: "+JSON.stringify(auth, null, 2))

    const actions = [{
        account: 'eosio',
        name: 'updateauth',
        authorization: [{
          actor: targetAccount,
          permission: "owner",
        }],
        data: {
          account: targetAccount,
          permission: permission_name,
          parent,
          auth
        },
    }]

    const res = await createMultisigProposal(proposerAccount, proposalName, actions, "owner")

    const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

    console.log("ESR for Approve: " + JSON.stringify(approveESR))
  
    const execESR = await createESRCodeExec({proposerAccount, proposalName})
  
    console.log("ESR for Exec: " + JSON.stringify(execESR))
    

  }
  
/**
 * Sets active and owner to GuardianAccountName and msig.seeds
 * @param {*} targetAccount 
 */
const setCGPermissions = async (targetAccount, permission_name, hot = false, propose = false) => {
  
  if (hot == "false" || hot == 0) {
    hot = false
  }

  console.log('setGuardiansPermissions on ' + targetAccount + " " + (hot ? "hot" : "test mode") + ( propose ? "PROPOSE " : ""))



    assert(permission_name == "active" || permission_name == "owner", "permission must be active or owner")

    const parent = permission_name == 'owner' ? "." : "owner"

    const { permissions } = await eos.getAccount(targetAccount)
    const perm = permissions.find(p => p.perm_name === permission_name)

    console.log("existing permission "+JSON.stringify(perm, null, 2))

    const { required_auth } = perm
    const { keys, accounts, waits } = required_auth

    let accountPermissions = [
        ...accounts,
      {
        permission: { 
          actor: GuardianAccountName, 
          permission: permission_name
        },
        weight: 1
      },
      {
        permission: { 
          actor: "msig.seeds", 
          permission: "eosio.code"
        },
        weight: 1
      }
    ]

    accountPermissions.sort((a, b) => {
        if (a.permission.actor <= b.permission.actor) {
          return -1
        }
        return 1
      })
    
    const auth = {
      threshold: 1,
      waits: [],
      accounts: accountPermissions,
      keys: []
    }


    const actions = [{
        account: 'eosio',
        name: 'updateauth',
        authorization: [{
          actor: targetAccount,
          permission: "owner",
        }],
        data: {
          account: targetAccount,
          permission: permission_name,
          parent,
          auth
        },
    }]
  
    const trxConfig = {
      blocksBehind: 3,
      expireSeconds: 30,
    }
  
    if (hot) {

      if (!propose) {
        let res = await eos.api.transact({
          actions
        }, trxConfig)
        const newPermissions = await eos.getAccount(targetAccount)
        console.log("new permissions on "+targetAccount+" "+JSON.stringify(newPermissions.permissions, null, 2))  
      } else {
        console.log("msig for new permissions: "+JSON.stringify(auth, null, 2))

        proposeMsig("illumination", "prop1111", targetAccount, actions, "owner")
      }
  
    } else {
      console.log("new permissions would be: "+JSON.stringify(auth, null, 2))
    }

}

const proposeDeploy = async (proposerAccount, proposalName, contractName) => {
  console.log('propose deployment of '+contractName)

  const api = eos.api

  const { code, abi } = await source(contractName)

  console.log("compiled code with length " + code.length)

  console.log("constructed abi with length " + abi.length)

  const contractAccount = accounts[contractName].account

  const setCodeData = {
    account: contractAccount,
    code: code.toString('hex'),
    vmtype: 0,
    vmversion: 0,
  }

  const setCodeAuth = [{
    actor: contractAccount,
    permission: 'active',
  }]

  const abiAsHex = await abiToHex(JSON.parse(abi))
  // console.log("hex abi: "+abiAsHex)

  const setAbiData = {
    account: contractAccount,
    abi: abiAsHex
  }

  const setAbiAuth = [{
    actor: contractAccount,
    permission: 'active',
  }]

  const actions = [{
    account: 'eosio',
    name: 'setcode',
    data: setCodeData,
    authorization: setCodeAuth
  }, {
    account: 'eosio',
    name: 'setabi',
    data: setAbiData,
    authorization: setAbiAuth
  }]


  const res = await createMultisigProposal(proposerAccount, proposalName, actions)

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

  console.log("ESR for Approve: " + JSON.stringify(approveESR))

  const execESR = await createESRCodeExec({proposerAccount, proposalName})

  console.log("ESR for Exec: " + JSON.stringify(execESR))

}

const abiToHex = (abi) => {
  const api = eos.api

  const buffer = new Serialize.SerialBuffer({
    textEncoder: api.textEncoder,
    textDecoder: api.textDecoder,
  })

  const abiDefinitions = api.abiTypes.get('abi_def')

  abi = abiDefinitions.fields.reduce(
    (acc, { name: fieldName }) =>
      Object.assign(acc, { [fieldName]: acc[fieldName] || [] }),
    abi
  )

  abiDefinitions.serialize(buffer, abi)
  const serializedAbiHexString = Buffer.from(buffer.asUint8Array()).toString('hex')

  return serializedAbiHexString
}

const getConstitutionalGuardians = async (permission_name = "active") => {
  const guardacct = GuardianAccountName
  const { permissions } = await eos.getAccount(guardacct)
  const activePerm = permissions.filter(item => item.perm_name == permission_name)
  const result = activePerm[0].required_auth.accounts
    .filter(item => item.permission.actor != "msig.seeds")
    .map(item => item.permission)
  //console.log("CG accounts: "+JSON.stringify(result, null, 2))
  return result
}

const getActivePermissionAccounts = async (account, permission_name = "active") => {
  const { permissions } = await eos.getAccount(account)
  const activePerm = permissions.filter(item => item.perm_name == permission_name)
  const result = activePerm[0].required_auth.accounts
    .filter(item => item.permission.permission == "active" || item.permission.permission == "owner")
    .map(item => item.permission)
  console.log("CG accounts: "+JSON.stringify(result, null, 2))
  return result
}

// take any input of actions, create a multisig proposal for guardians from it!

const createMultisigProposal = async (proposerAccount, proposalName, actions, permission = "active") => {

    const api = eos.api

    const serializedActions = await api.serializeActions(actions)
  
    console.log("====== PROPOSING ======")
  
    const guardians = await getConstitutionalGuardians(permission)
  
    console.log("requested permissions: "+permission+ " " + JSON.stringify(guardians.map(item => item.actor)))
  
    const info = await eos.api.rpc.get_info()
    const head_block = await eos.api.rpc.get_block(info.last_irreversible_block_num)
    // Note: Only 1 hour expiration - the problem with longer expirations is that we may get a "transaction expiration too far
    // in the future" error. 
    const expiration = Serialize.timePointSecToDate(Serialize.dateToTimePointSec(head_block.timestamp) + 3600 )
  
    const proposeInput = {
      proposer: proposerAccount,
      proposal_name: proposalName,
      requested: guardians,
      trx: {
        expiration: expiration,
        ref_block_num: 0,
        ref_block_prefix: 0,
        max_net_usage_words: 0,
        max_cpu_usage_ms: 0,
        delay_sec: 0,
        context_free_actions: [],
        actions: serializedActions,
        transaction_extensions: []
      }
    };
  
    //console.log('send propose ' + JSON.stringify(proposeInput))
    // console.log("propose action")
  
    const auth = [{
      actor: proposerAccount,
      permission: "active",
    }]
  
    const propActions = [{
      account: 'msig.seeds',
      name: 'propose',
      authorization: auth,
      data: proposeInput
    }]
  
    const trxConfig = {
      blocksBehind: 3,
      expireSeconds: 30,
    }
  
    //console.log("msig proposal: "+JSON.stringify(propActions, null,2))

    let res = await api.transact({
      actions: propActions
    }, trxConfig)

    return res
}

const issueHypha = async (quantity, proposerAccount, propName) => {

  const actions = [{
    account: 'hypha.hypha',
    name: 'issue',
    authorization: [{
      "actor": "dao.hypha",
      "permission": "active"
    }],
    data: {
      "to": "dao.hypha",
      "quantity": quantity,
      "memo": "[POLICY] FinFlow Authority to mint 1,000,000 HYPHA tokens"
    },
  }]

  const proposeESR = await createMultisigProposalESR(propName, proposerAccount, actions, "dao.hypha")

  console.log("Propose ESR: " + JSON.stringify(proposeESR, null, 2))

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("approveESR: " + JSON.stringify(approveESR, null, 2))

  const execESR = await createESRCodeExec({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("execESR: " + JSON.stringify(execESR, null, 2))

  const cancelESR = await createESRCodeCancel({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("CANCEL: " + JSON.stringify(cancelESR, null, 2))

}

const sendHypha = async (quantity, recepient, proposerAccount, propName) => {

  const actions = [{
    account: 'hypha.hypha',
    name: 'transfer',
    authorization: [{
      "actor": "dao.hypha",
      "permission": "active"
    }],
    data: {
      "from": "dao.hypha",
      "to": recepient,
      "quantity": quantity,
      "memo": ""
    },
  }]

  const proposeESR = await createMultisigProposalESR(propName, proposerAccount, actions, "dao.hypha")

  console.log("Propose ESR: " + JSON.stringify(proposeESR, null, 2))

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("approveESR: " + JSON.stringify(approveESR, null, 2))

  const execESR = await createESRCodeExec({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("execESR: " + JSON.stringify(execESR, null, 2))

  const cancelESR = await createESRCodeCancel({proposerAccount, proposalName: propName, msigAccount: "eosio.msig"})
  console.log("CANCEL: " + JSON.stringify(cancelESR, null, 2))

}

const createMultisigProposalESR = async (proposalName, proposerAccount, actions, permissionAccount, permission = "active") => {

  const api = eos.api

  const serializedActions = await api.serializeActions(actions)

  console.log("====== PROPOSING ======")

  console.log("actions: " + JSON.stringify(actions, null, 2))

  const guardians = await getActivePermissionAccounts(permissionAccount, permission)

  console.log("requested permissions: "+permission+ " " + JSON.stringify(guardians.map(item => item.actor)))

  const info = await eos.api.rpc.get_info()
  const head_block = await eos.api.rpc.get_block(info.last_irreversible_block_num)
  // Note: Only 1 hour expiration - the problem with longer expirations is that we may get a "transaction expiration too far
  // in the future" error. 
  const expiration = Serialize.timePointSecToDate(Serialize.dateToTimePointSec(head_block.timestamp) + 3600 * 24 * 7 )
  
  const proposeInput = {
    proposer: proposerAccount,
    proposal_name: proposalName,
    requested: guardians,
    trx: {
      expiration: expiration,
      ref_block_num: 0,
      ref_block_prefix: 0,
      max_net_usage_words: 0,
      max_cpu_usage_ms: 0,
      delay_sec: 0,
      context_free_actions: [],
      actions: serializedActions,
      transaction_extensions: []
    }
  };

  //console.log('send propose ' + JSON.stringify(proposeInput))
  // console.log("propose action")

  const propActions = [{
    account: 'eosio.msig',
    name: 'propose',
    authorization: [{
      actor: proposerAccount,
      permission: "active",
    }],
    data: proposeInput
  }]

  console.log("msig proposal: "+JSON.stringify(propActions, null,2))

  const proposeEsr = await createESRWithActions({actions: propActions, title: "Create proposal " + proposalName})

  return proposeEsr
}

const createESRCodeApprove = async ({proposerAccount, proposalName, msigAccount = "msig.seeds"}) => {

  const approveActions = [{
    account: msigAccount,
    name: 'approve',
    data: {
      proposer: proposerAccount,
      proposal_name: proposalName,
      level: {
        actor: authPlaceholder,
        permission: 'active'
      }
    },
    authorization: [{
      actor: authPlaceholder,
      permission: 'active'
    }]
  }]

  return createESRWithActions({actions: approveActions, title: "APPROVE proposal "+proposalName})
}

const createESRCodeExec = async ({proposerAccount, proposalName, msigAccount = "msig.seeds"}) => {

  const execActions = [{
    account: msigAccount,
    name: 'exec',
    data: {
      proposer: proposerAccount,
      proposal_name: proposalName,
      executer: authPlaceholder
    },
    authorization: [{
      actor: authPlaceholder,
      permission: 'active'
    }]
  }]

  return createESRWithActions({actions: execActions, title: "Execute proposal "+proposalName})
}

const createESRCodeCancel = async ({proposerAccount, proposalName, msigAccount = "msig.seeds"}) => {
  const execActions = [{
    account: msigAccount,
    name: 'cancel',
    data: {
      proposer: proposerAccount,
      proposal_name: proposalName,
      canceler: authPlaceholder
    },
    authorization: [{
      actor: authPlaceholder,
      permission: 'active'
    }]
  }]

  return createESRWithActions({actions: execActions, title: "Cancel proposal "+proposalName})
}

const createESRCodeTransfer = async ({recepient, amount, memo}) => {

  const actions = [{
    account: 'token.seeds',
    name: 'transfer',
    data: {
      from: authPlaceholder,
      to: recepient,
      amount: amount.toFixed(4) + " SEEDS",
      memo: memo
    },
    authorization: [{
      actor: authPlaceholder,
      permission: 'active'
    }]
  }]

  return createESRWithActions({actions: actions})
}


const createESRWithActions = async ({actions, title = "Generating ESR Code"}) => {

  console.log("========= " + title + " ===========")
  
  const esr_uri = "https://api-esr.hypha.earth/qr"
  const body = {
    actions
  }

  //console.log("actions: "+JSON.stringify(body, null, 2))

  const rawResponse = await fetch(esr_uri, {
    method: 'POST',
    headers: {
      'Accept': 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  });

  const parsedResponse = await rawResponse.json();

  //console.log("parsed response "+JSON.stringify(parsedResponse))

  return parsedResponse
}

module.exports = { proposeDeploy, proposeChangeGuardians, setCGPermissions, proposeKeyPermissions, issueHypha, sendHypha }