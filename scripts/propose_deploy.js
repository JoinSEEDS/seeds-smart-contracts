const assert = require('assert')
const zlib = require('zlib')
const fetch = require('node-fetch')

const eosjs = require('eosjs')
const { Serialize } = eosjs
const { SigningRequest } = require('eosio-signing-request')
const { source } = require('./deploy')
const { accounts, eos } = require('./helper')

const authPlaceholder = "............1"

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

const getConstitutionalGuardians = async () => {
  const guardacct = "cg.seeds"
  const { permissions } = await eos.getAccount(guardacct)
  const activePerm = permissions.filter(item => item.perm_name == "active")
  const result = activePerm[0].required_auth.accounts
    .filter(item => item.permission.actor != "msig.seeds")
    .map(item => item.permission)
  //console.log("CG accounts: "+JSON.stringify(result, null, 2))
  return result
}

const proposeDeploy = async (contractName, proposalName, proposerAccount) => {
  console.log('starting deployment')

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

  const serializedActions = await api.serializeActions(actions)

  // console.log("SER Actions:")
  // console.log(serializedActions)

  console.log("====== PROPOSING ======")

  const guardians = await getConstitutionalGuardians()

  console.log("requested permissions active: "+JSON.stringify(guardians.map(item => item.actor)))

  const proposeInput = {
    proposer: proposerAccount,
    proposal_name: proposalName,
    requested: guardians,
    trx: {
      expiration: '2021-09-14T16:39:15',
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
  console.log("propose action")

  const auth = [{
    actor: proposerAccount,
    permission: 'active',
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

  let res = await api.transact({
    actions: propActions
  }, trxConfig)

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

  console.log("ESR for Approve: " + JSON.stringify(approveESR))

  const execESR = await createESRCodeExec({proposerAccount, proposalName})

  console.log("ESR for Exec: " + JSON.stringify(execESR))

}

const createESRCodeApprove = async ({proposerAccount, proposalName}) => {

  const approveActions = [{
    account: 'msig.seeds',
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

  return createESRWithActions({actions: approveActions})
}

const createESRCodeExec = async ({proposerAccount, proposalName}) => {

  const execActions = [{
    account: 'msig.seeds',
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

  return createESRWithActions({actions: execActions})
}

const createESRWithActions = async ({actions}) => {

  console.log("========= Generating ESR Code ===========")
  
  const esr_uri = "https://api-esr.hypha.earth/qr"
  const body = {
    "actions": actions
  }

  const rawResponse = await fetch(esr_uri, {
    method: 'POST',
    headers: {
      'Accept': 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  });

  const parsedResponse = await rawResponse.json();

  return parsedResponse
}

module.exports = proposeDeploy