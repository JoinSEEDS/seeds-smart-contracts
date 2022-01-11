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

const distributionDataFile = "./scripts/h2/HVOICE_accounts_balances_202201120004.csv"

const processDataFile = async () => {
  const fileStream = fs.createReadStream(distributionDataFile);

  const rl = readline.createInterface({
    input: fileStream,
    crlfDelay: Infinity
  });

  var lineCount = 0

  var data = []

  for await (const line of rl) {

    if (lineCount == 0) {
      // skip
    } else {
      const components = line.split(",")

      console.log(`account: ${components[0]}`);

      data.push({
        account: components[0],
        amount: components[1],
      })
    }

    lineCount++
  }

  return data
}

const voiceResetAction = (account) => {

  return {
    "account": "voice.hypha",
    "name": "voicereset",
    "authorization": [{
      "actor": "voice.hypha",
      "permission": "active"
    }
    ],
    "data": {
      "owner": account,
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


const voicereset = async () => {
  console.log("getting new token distributions");
  const data = await processDataFile()

  const batchMaxLength = 119

  let batchNum = 1

  batchData = []

  results = []

  itemCount = 0
  skipped = 0

  for (item of data) {

    if (item.amount == 1) {
      console.log("skipping amount 1 account: "+item.account)
      skipped++
      continue
    }
    itemCount++
    batchData.push(item)
    if (batchData.length > batchMaxLength || item == data[data.length-1]) {
      results.push(await voiceResetBatch(batchNum, batchData))
      batchData = []
      batchNum++
    }
  }

  console.log("items: "+itemCount)
  console.log("skipped: "+skipped)

  // console.log("URLs ")
  // for (r of results) {
  //   console.log("Batch "+r.batchNum + " "+ r.url)
  // }

  console.log("ESR ")
  for (r of results) {
    console.log("Batch "+r.batchNum + " "+ r.esr.esr)
  }

  console.log("QR ")
  for (r of results) {
    console.log("Batch "+r.batchNum + " "+ r.esr.qr)
  }

}

const voiceResetBatch = async (batchNum, data) => {
  let actions = []
  let sum = 0
  let intsum = 0

  console.log("create batch " + batchNum)

  for (item of data) {
    actions.push(
      voiceResetAction(item.account)
    )
  }

  proposerAccount = "illumination"
  propnumber = "xabcdefghijklmnopq"
  proposalName = "hip2voice" + propnumber.charAt(batchNum)

  console.log("batch " + batchNum + " has " + actions.length + " ACTIONS: " + JSON.stringify(actions, null, 2))

  // const proposeESR = await createMultisigPropose(proposerAccount, proposalName, "dao.hypha", actions)
  // console.log("ESR " + batchNum + " for propose to " + proposalName + ": " + JSON.stringify(proposeESR, null, 2))

  const esr = await createESRWithActions({ actions: actions })

  return {
    batchNum,
    esr,
  }

}



program
  .command('voicereset')
  .description('HIP2 voice reset')
  .action(async function () {
    await voicereset()
  })






program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}

