#!/usr/bin/env node
const fetch = require("node-fetch");
const program = require('commander')
const fs = require('fs')
const readline = require('readline');

const host = "https://api.telosfoundation.io"

const { eos, getTableRows, accounts } = require("./helper");
const { min } = require("ramda");
const { parse } = require("path");
const { action } = require("commander");

const logDir = "log"

const distributionDataFile = "./scripts/h2/HIP Hypha^1 Staking, Bonuses - Exportable.tsv"

const processDataFile = async () => {
  const fileStream = fs.createReadStream(distributionDataFile);

  const rl = readline.createInterface({
    input: fileStream,
    crlfDelay: Infinity
  });
  // Note: we use the crlfDelay option to recognize all instances of CR LF
  // ('\r\n') in input.txt as a single line break.

  var lineCount = 0

  var data = []

  for await (const line of rl) {
    // Each line in input.txt will be successively available here as `line`.
    console.log(`Line from file: ${line}`);

    if (lineCount == 0) {
      // skip
    } else {
      const components = line.split("\t")

      data.push({
        account: components[0],
        name: components[1],
        current: components[2],
        multiplier: components[3],
        after: components[4],
      })
    }

    lineCount++
  }

  return data
}

const getBalance = async ({
  user,
  code = 'token.seeds',
  symbol = 'SEEDS',
}) => {

  const params = {
    "json": "true",
    "code": code,
    "scope": user,
    "table": 'accounts',
    'lower_bound': symbol,
    'upper_bound': symbol,
    "limit": 10,
  }

  const url = host + "/v1/chain/get_table_rows"
  const rawResponse = await fetch(url, {
    method: 'POST',
    headers: {
      'Accept': 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(params)
  });
  const res = await rawResponse.json();
  //console.log("res: "+JSON.stringify(res, null, 2))
  // res: {"rows":[{"balance":"22465.0000 SEEDS"}],"more":false,"next_key":""}
  if (res.rows != null && res.rows.length > 0 && res.rows[0].balance != null) {
    const balance = res.rows[0].balance
    return balance
  } else {
    console.log("account " + user + " is invalid " + JSON.stringify(res, null, 2))
    return null
  }

}

const reduceAction = (account, amount) => {

  return {
    "account": "token.hypha",
    "name": "reduce",
    "authorization": [{
      "actor": "dao.hypha",
      "permission": "active"
    }
    ],
    "data": {
      "account": account,
      "quantity": amount + " HYPHA"
    },
  }

}

const getEOSTimePoint = (date) => {
  return date.toISOString().replace('Z', '')
}

const lockAction = (account, amount) => {

  // const expiration = new Date()
  // expiration.setFullYear(expiration.getFullYear() + 3)
  // const expirationString = getEOSTimePoint(expiration)

  const expirationString  = "2025-01-11T11:11:44.000"

  return {
    "account": "costak.hypha",
    "name": "createlock",
    "authorization": [{
      "actor": "dao.hypha",
      "permission": "active"
    }
    ],
    "data": {
      "sponsor": "dao.hypha",
      "beneficiary": account,
      "quantity": amount + " HYPHA",
      "notes": 'HIP1 redistribution event',
      "expiration_date": expirationString 
    },
  }

}

const transferAction = ({ beneficiary, amount, memo }) => {

  return {
    "account": "token.hypha",
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
      "memo": "HIP1 redistribution event"
    },
  }

}

const issueAction = (amount) => {

  return {
    "account": "token.hypha",
    "name": "issue",
    "authorization": [{
      "actor": "dao.hypha",
      "permission": "active"
    }
    ],
    "data": {
      "to": "dao.hypha",
      "quantity": amount.toFixed(2) + " HYPHA",
      "memo": "HIP1 redistribution event"
    },
  }

}

const getApprovers = async (account, permission_name = "active") => {
  const { permissions } = await eos.getAccount(account)
  const activePerm = permissions.filter(item => item.perm_name == permission_name)
  const result = activePerm[0].required_auth.accounts
    .filter(item => item.permission.actor != account)
    .map(item => item.permission)
  //console.log("approver accounts: " + JSON.stringify(result, null, 2))
  return result
}

const createMultisigPropose = async (proposerAccount, proposalName, contract, actions, permission = "active") => {

  const api = eos.api

  const serializedActions = await api.serializeActions(actions)

  requestedApprovals = await getApprovers(contract)

  console.log("====== PROPOSING ======")

  console.log("requested permissions: " + permission + " " + JSON.stringify(requestedApprovals))

  const proposeInput = {
    proposer: proposerAccount,
    proposal_name: proposalName,
    requested: requestedApprovals,
    trx: {
      expiration: '2022-01-27T16:39:15',
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

  return createESRWithActions({ actions: propActions })

}

const createESRWithActions = async ({ actions }) => {

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


const distribute = async () => {
  console.log("getting new token distributions");
  const data = await processDataFile()

  const batchMaxLength = 17

  let batchNum = 1

  batchData = []

  results = []

  for (item of data) {
    batchData.push(item)
    if (batchData.length > batchMaxLength || item == data[data.length-1]) {
      results.push(await distributeBatch(batchNum, batchData))
      batchData = []
      batchNum++
    }
  }

  console.log("URLs ")
  for (r of results) {
    console.log("Batch "+r.batchNum + " "+ r.url)
  }

  console.log("ESR ")
  for (r of results) {
    console.log("Batch "+r.batchNum + " "+ r.esr)
  }

  console.log("QR ")
  for (r of results) {
    console.log("Batch "+r.batchNum + " "+ r.qr)
  }

}

const distributeBatch = async (batchNum, data) => {
  let actions = []
  let sum = 0
  let intsum = 0

  console.log("create batch " + batchNum)

  for (item of data) {

    /// 1 destroy the balance
    actions.push(
      reduceAction(item.account, item.current)
    )

    /// 2 send the new balance to the CS contract
    actions.push(
      lockAction(item.account, item.after)
    )

    const itemAfterX100 = parseInt(item.after.replace(".", ""))

    const floatAmount = parseFloat(item.after)

    if (floatAmount.toFixed(2) != item.after) {
      console.log("ERROR on " + item.after)
      throw "fatal - math error" + parseFloat(item.after) + " != " + item.after
    }

    sum = sum + floatAmount

    intsum += itemAfterX100

  }

  console.log(" total sum: " + sum)
  console.log(" total intsum: " + intsum)

  // issue HYPHA
  actions.splice(0, 0, issueAction(sum))

  // transfer to lock account
  actions.splice(1, 0, transferAction({ beneficiary: "costak.seeds", amount: sum, memo: "H^2 redistribution" }))

  proposerAccount = "illumination"
  propnumber = "xabcdefghijklmnopq"
  proposalName = "h2dist" + propnumber.charAt(batchNum)

  console.log("batch " + batchNum + " has " + actions.length + " ACTIONS: " + JSON.stringify(actions, null, 2))

  const proposeESR = await createMultisigPropose(proposerAccount, proposalName, "dao.hypha", actions)

  console.log("ESR " + batchNum + " for propose to " + proposalName + ": " + JSON.stringify(proposeESR, null, 2))

  return {
    batchNum,
    proposalName,
    url: "https://telos.bloks.io/msig/"+proposerAccount+"/"+proposalName,
    esr: proposeESR.esr,
    qr: proposeESR.qr,

  }
}

const verify = async () => {
  console.log("verifying data in spreadsheet with chain data");
  const data = await processDataFile()

  //console.log("data: "+JSON.stringify(data, null, 2))

  /// Check if the balance is accurate

  let sum = 0

  for (item of data) {

    const balance = await getBalance({
      user: item.account,
      code: "token.hypha",
      symbol: "HYPHA"
    })

    const amountString = item.current + " HYPHA"

    if (balance != amountString) {
      console.log("account: " + item.account + " expected: " + item.current + " actual: " + balance)
    }

    sum = sum + parseFloat(item.after)
  }

  console.log("Total sum: " + sum)

  console.log("Verify done.")

}

program
  .command('distribute')
  .description('distribute HYPHA^2')
  .action(async function () {
    await distribute()
  })

program
  .command('verify')
  .description('verify HYPHA^2 data with chain')
  .action(async function () {
    await verify()
  })





program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}

