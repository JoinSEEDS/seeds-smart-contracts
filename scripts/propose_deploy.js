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

  const proposeInput = {
    proposer: proposerAccount,
    proposal_name: proposalName,
    requested: [ // NOTE: Ignored
      {
        actor: proposerAccount,
        permission: "active"
      },
      {
        actor: 'seedsuseraaa',
        permission: 'active'
      },
      {
        actor: 'seedsuserbbb',
        permission: 'active'
      }
    ],
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

  await createESRCode({proposerAccount, proposalName})

}

const createESRCode = async ({proposerAccount, proposalName}) => {

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

  // The below seems to create an ESR that eosio.to can't parse for some reason
  // console.log('approve actions payload for ' + contractName)
  // assert(approveActions[0].data.proposer == contractName, "approve invalid proposal")
  // const chainInfo = await eos.api.rpc.get_info()
  // const headBlock = await eos.api.rpc.get_block(chainInfo.last_irreversible_block_num)
  // const chainId = chainInfo.chain_id

  // const approveTransaction = await buildTransaction({
  //   actions: approveActions,
  //   headBlock: headBlock
  // })
  // console.log('approve transaction will be expired at ' + approveTransaction.expiration.toString())
  // assert(new Date(approveTransaction.expiration) > new Date(), "approve transaction expired")

  // const request = await SigningRequest.create({
  //   transaction: approveTransaction,
  //   chainId: chainId
  // }, getEsrOpts())

  // console.log('signing request can be shared with guardians to approve deployment of the contract ' + contractName)

  // const uri = request.encode()

  //console.log(`https://eosio.to/${uri.replace('esr://', '')}`)

  console.log("=========================================")
  console.log("========= Generating ESR Code ===========")
  console.log("=========================================")
  
  const esr_uri = "https://api-esr.hypha.earth/qr"
  const body = {
    "actions": approveActions
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

  console.log("ESR response: " + JSON.stringify(parsedResponse))

  return parsedResponse
}

const buildTransaction = ({ actions, headBlock }) => ({
  expiration: Serialize.timePointSecToDate(Serialize.dateToTimePointSec(headBlock.timestamp) + 3600 * 24),
  ref_block_num: headBlock.block_num & 0xffff,
  ref_block_prefix: headBlock.ref_block_prefix,
  max_net_usage_words: 0,
  delay_sec: 0,
  context_free_actions: [],
  actions: actions,
  transaction_extensions: [],
  signatures: [],
  context_free_data: []
})

const getEsrOpts = () => ({
  textEncoder: eos.api.textEncoder,
  textDecoder: eos.api.textDecoder,
  zlib: {
    deflateRaw: (data) => new Uint8Array(zlib.deflateRawSync(Buffer.from(data))),
    inflateRaw: (data) => new Uint8Array(zlib.inflateRawSync(Buffer.from(data))),
  },
  abiProvider: {
    getAbi: async (account) => (await eos.api.getAbi(account))
  }
})

module.exports = proposeDeploy