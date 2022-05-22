const assert = require('assert')
const zlib = require('zlib')
const fetch = require('node-fetch')

const eosjs = require('eosjs')
const { Serialize } = eosjs
const { SigningRequest } = require('eosio-signing-request')
const { source } = require('./deploy')
const { accounts, eos } = require('./helper')

const authPlaceholder = "............1"


const beneficiaries = [
["atmalife2312",	345],
["gguijarro123",	27],
["imprvsociety",	236],
["jasondoh1155",	380],
["juliomonteir",	323],
["khempoudel12",	323],
["leonieherma1",	244],
["lukegravdent",	1152],
["markflowfarm",	464],
["massimohypha",	95],
["matteotangi1",	57],
["nomik22kimon",	1102],
["seedspiyush2",	898],
["sienaraa3535",	243],
["thealchemist",	1507],
]


const transferAction = (beneficiary, amount) => {

  return  {
    "account": "hypha.hypha",
    "name": "transfer",
    "authorization": [{
        "actor": "dao.hypha",
        "permission": "active"
      }
    ],
    "data": {
      "from": "dao.hypha",
      "to": beneficiary,
      "quantity": amount.toFixed(2) + " HYPHA",
      "memo": "Hypha payout correction 2021/12"
    },
  }

}

const issueAction = (amount) => {

  return  {
    "account": "hypha.hypha",
    "name": "issue",
    "authorization": [{
        "actor": "dao.hypha",
        "permission": "active"
      }
    ],
    "data": {
      "to": "dao.hypha",
      "quantity": amount.toFixed(2) + " HYPHA",
      "memo": "Hypha payout correction 2021/12"
    },
  }

}

const proposePayout = async () => {
  console.log('propose payout ')

  const api = eos.api

  var total = 0

  beneficiaries.forEach( (b) => total += b[1])

  console.log("TOTAL: " + total.toFixed(2)  + " HYPHA")

  const actions = []
  
  actions.push(issueAction(total))

  beneficiaries.forEach( (b) => actions.push(transferAction(b[0], b[1])))

  proposerAccount = "illumination"
  proposalName = "amend1"

  console.log("ACTIONS: "+JSON.stringify(actions, null, 2))

  const proposeESR = await createMultisigPropose(proposerAccount, proposalName, "dao.hypha", actions)

  console.log("ESR for Propose: " + JSON.stringify(proposeESR, null, 2))

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

  console.log("ESR for Approve: " + JSON.stringify(approveESR, null, 2))

  const execESR = await createESRCodeExec({proposerAccount, proposalName})

  console.log("ESR for Exec: " + JSON.stringify(execESR, null, 2))

}

const getApprovers = async (account, permission_name = "active") => {
  const { permissions } = await eos.getAccount(account)
  const activePerm = permissions.filter(item => item.perm_name == permission_name)
  const result = activePerm[0].required_auth.accounts
    .filter(item => item.permission.actor != account)
    .map(item => item.permission)
  console.log("CG accounts: "+JSON.stringify(result, null, 2))
  return result
}

// take any input of actions, create a multisig proposal for guardians from it!

const createMultisigPropose = async (proposerAccount, proposalName, contract, actions, permission = "active") => {

    const api = eos.api

    const serializedActions = await api.serializeActions(actions)
  
    requestedApprovals = await getApprovers(contract)

    console.log("====== PROPOSING ======")
    
    console.log("requested permissions: "+permission+ " " + JSON.stringify(requestedApprovals))
  
    const info = await rpc.get_info();
    const head_block = await rpc.get_block(info.last_irreversible_block_num);
    const chainId = info.chain_id;

    const expiration = Serialize.timePointSecToDate(Serialize.dateToTimePointSec(head_block.timestamp) + 3600 * 24 * 7)

    const proposeInput = {
      proposer: proposerAccount,
      proposal_name: proposalName,
      requested: requestedApprovals,
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
    
    const auth = [{
      actor: proposerAccount,
      permission: "active",
    }]
  
    const propActions = [{
      account: 'eosio.msig',
      name: 'propose',
      authorization: auth,
      data: proposeInput
    }]
    
    return createESRWithActions({actions: propActions})

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

proposePayout()
