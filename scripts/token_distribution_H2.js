#!/usr/bin/env node
const fetch = require("node-fetch");
const program = require('commander')
const fs = require('fs')
const readline = require('readline');

const host = "https://api.telosfoundation.io"

const { eos, getTableRows, accounts } = require("./helper");
const { min } = require("ramda");
const { parse } = require("path");

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

  //console.log("balance: "+JSON.stringify(balance))
}


function timeStampString() {
  var date = new Date()
  var hours = date.getHours();
  var minutes = date.getMinutes();
  var month = date.getMonth() + 1;
  var day = date.getDate()

  hours = hours < 10 ? '0' + hours : hours;
  minutes = minutes < 10 ? '0' + minutes : minutes;
  day = day < 10 ? '0' + day : day;
  month = month < 10 ? '0' + month : month;

  res = date.getFullYear() + month + day + hours + minutes;

  return res
}


/** Raw call to `/v1/chain/get_table_by_scope` */
const get_table_by_scope = async ({
  code,
  table,
  lower_bound = '',
  upper_bound = '',
  limit = 10,
}) => {

  const url = host + '/v1/chain/get_table_by_scope'

  const params = {
    code,
    table,
    lower_bound,
    upper_bound,
    limit,
  }

  const rawResponse = await fetch(url, {
    method: 'POST',
    headers: {
      'Accept': 'application/json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(params)
  });
  const res = await rawResponse.json();
  return res

}

const getBalanceObjectFor = async (account, code, symbol) => {
  const balance = await getBalance({
    user: account,
    code: code,
    symbol: symbol
  })
  if (balance != null) {
    return {
      account: account,
      balance: balance,
      date: new Date().toISOString()
    }
  } else {
    return null
  }
}

const addBalances = async (balances, accounts, contract, symbol) => {
  var futures = []
  accounts.forEach((acct) => futures.push(getBalanceObjectFor(acct, contract, symbol)))
  var results = await Promise.all(futures)

  results.forEach(res => {
    if (res != null) {
      console.log("adding balance " + JSON.stringify(res, null, 2))
      balances[res.account] = res.balance
      // balances.push(res)
    }
  });

}


const burnAction = (account, amount) => {

  return  {
    "account": "token.hypha",
    "name": "burn",
    "authorization": [{
        "actor": "dao.hypha",
        "permission": "active"
      }
    ],
    "data": {
      "account": account,
      "quantity": amount + " HYPHA",
      //"memo": "Hypha H^2 Redistribution"
    },
  }

}

const createLockAction = (account, amount) => {

  // TBD

  return  {
    "account": "costak.hypha", // TBD
    "name": "createlock",
    "authorization": [{
        "actor": "dao.hypha",
        "permission": "active"
      }
    ],
    "data": {
      "account": account,
      "quantity": amount + " HYPHA",
      //"memo": "Hypha H^2 Redistribution"
    },
  }

}

const transferAction = ({beneficiary, amount, memo}) => {

  return  {
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
      "memo": memo
    },
  }

}

const issueAction = (amount) => {

  return  {
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
      "memo": "Hypha payout correction 2021/12"
    },
  }

}

const createMultisigPropose = async (proposerAccount, proposalName, contract, actions, permission = "active") => {

  const api = eos.api

  const serializedActions = await api.serializeActions(actions)

  requestedApprovals = await getApprovers(contract)

  console.log("====== PROPOSING ======")
  
  console.log("requested permissions: "+permission+ " " + JSON.stringify(requestedApprovals))

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
  
  return createESRWithActions({actions: propActions})

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


const distribute = async () => {
  console.log("getting new token distributions");
  const data = await processDataFile()

  //console.log("data: "+JSON.stringify(data, null, 2))

  // {
  //   "account": "solomonx3333",
  //   "name": "David Solomon",
  //   "current": "8229.66",
  //   "multiplier": "5.00",
  //   "after": "41148.30"
  // },

  /// 1 - Check if the balance is accurate

  let actions = []
  let sum = 0

  for (item of data) {

    const balance = await getBalance({
      user: item.account,
      code: "token.hypha",
      symbol: "HYPHA"
    })

    const amountString = item.current + " HYPHA"

    if (balance != amountString) {
      console.log("account: "+item.account)
      console.log("current: "+item.current)
      console.log("balance: "+balance)
    }

    /// 2 - destroy the balance
    actions.push(
      burnAction(item.account, amountString)
    )

    /// 3 - send the new balance to the CS contract
    actions.push(
      createLockAction(item.account, item.after)
    )

    sum = sum + parseFloat(item.after)



  }

  console.log("sum: "+sum)
  
  // issue HYPHA
  actions.push(
    issueAction(sum)
  )

  // transfer to lock account
  actions.push(
    transferAction({beneficiary: "vast.seeds", amount: sum, memo: "H^2 redistribution"})
  )

  proposerAccount = "illumination"
  proposalName = "h2dist"

  console.log("ACTIONS: "+JSON.stringify(actions, null, 2))

  const proposeESR = await createMultisigPropose(proposerAccount, proposalName, "dao.hypha", actions)

  console.log("ESR for Propose: " + JSON.stringify(proposeESR, null, 2))

  const approveESR = await createESRCodeApprove({proposerAccount, proposalName})

  console.log("ESR for Approve: " + JSON.stringify(approveESR, null, 2))

  const execESR = await createESRCodeExec({proposerAccount, proposalName})

  console.log("ESR for Exec: " + JSON.stringify(execESR, null, 2))



}

program
  .command('distribute')
  .description('distribute HYPHA^2')
  .action(async function () {
    await distribute()
  })





program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}

