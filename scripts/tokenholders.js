#!/usr/bin/env node
const fetch = require("node-fetch");
const program = require('commander')
const fs = require('fs')

const host = "https://node.hypha.earth"

const { eos, getTableRows } = require("./helper")

const snapshotDir = "snapshots"
const snapshotDirPath = "snapshots/"

const getBalance = async (user) => {
  // const balance = await eos.getCurrencyBalance("token.seeds", user, 'SEEDS')

  const params = {
    "json": "true",
    "code": 'token.seeds',
    "scope": user,
    "table": 'accounts',
    'lower_bound': 'SEEDS',
    'upper_bound': 'SEEDS',
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
    console.log("account "+user+" is invalid "+JSON.stringify(res, null, 2))
    return null
  }

  //console.log("balance: "+JSON.stringify(balance))
}

const getPlanteBalances = async (lower_bound) => {


  console.log("getting planted "+lower_bound)

  const params = {
    "json": "true",
    "code": 'harvst.seeds',
    "scope": 'harvst.seeds',
    "table": 'planted',
    'lower_bound': lower_bound,
    "limit": 1000,
    
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

  return res
}

const getPayments = async (lower_bound) => {

  console.log("getting purchases "+lower_bound)

  const params = {
    "json": true,
    "code": "tlosto.seeds",
    "scope": "tlosto.seeds",
    "table": "payhistory",
    "table_key": "",
    "lower_bound": lower_bound,
    "upper_bound": "",
    "limit": 1000,
    "key_type": "",
    "index_position": "",
    "encode_type": "dec",
    "reverse": false,
    "show_payer": false
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
  return res
}


function timeStampString() {
  var date = new Date()
  var hours = date.getHours();
  var minutes = date.getMinutes();
  var month = date.getMonth()+1;
  minutes = minutes < 10 ? '0'+minutes : minutes;
  month = month < 10 ? '0'+month : month;
  var strTime = hours + '_' + minutes;
  return date.getFullYear() + "-" + month + "-" + date.getDate() + "_" + strTime;
}

const allPlanted = async () => {

  initDir()

  var more = true
  var lower_bound = 0

  var planteds = []

  console.log(timeStampString() )

  while (more) {
    const res = await getPlanteBalances(lower_bound)
    
    console.log("result: "+JSON.stringify(res, null, 2))

    res.rows.forEach(item => { 
      planteds.push(item)
    });

    lower_bound = parseInt(res.next_key) 
    
    more = res.more != "false" && res.more != ""

    console.log("planteds: "+planteds.length  + " next "+lower_bound)

  }

  fs.writeFileSync(snapshotDirPath + 'planted_balances_'+ timeStampString() +'.json', JSON.stringify(planteds, null, 2))

}

const allPayments = async () => {
  var more = true
  var lower_bound = 0

  var payments = []
  var paymentsCSV = ""
  var totalsMap = {}
  var uniqueAccounts = []

  console.log(timeStampString() )

  initDir()

  while (more) {
    const res = await getPayments(lower_bound)
    
    //console.log("result: "+JSON.stringify(res, null, 2))

    res.rows.forEach(item => { 
      var usdValue = item.multipliedUsdValue / 10000.0
      var usdPerSeeds = 0.01
      var seedsPerUSD = 100 // store both values to prevent rounding errors
      var seedsValue = (seedsPerUSD * usdValue)
      var account = item.recipientAccount

      payments.push(item)
      // "id": 0,
      // "recipientAccount": "illumination",
      // "paymentSymbol": "EOS",
      // "paymentId": "f415d689c0bc0b3d83ea8f25af8dd22e30fe7fa2848e0254a4c5b87682670384",
      // "multipliedUsdValue": 2
      item["usdPerSeeds"] = usdPerSeeds
      item["seedsPerUSD"] = seedsPerUSD
      item["usdValue"] = usdValue.toFixed(2)
      item["seedsValue"] = seedsValue
      item["seeds"] = seedsValue.toFixed(4) + " SEEDS"

      paymentsCSV = paymentsCSV 
        + item.id + "," 
        + account + ","
        + usdValue.toFixed(2) + ","
        + item.seedsPerUSD + ","
        + item.seeds + ","
        + item.paymentSymbol + "," 
        + item.paymentId  +"\n"

      mapEntry = totalsMap[account]

      if (mapEntry) {
        mapEntry.items.push(item)
        mapEntry.total += seedsValue
        mapEntry.totalSeeds = mapEntry.total.toFixed(4) + " SEEDS"
      } else {
        uniqueAccounts.push(account)
        totalsMap[account] = {
          items: [item],
          total: seedsValue,
          totalSeeds: seedsValue.toFixed(4) + " SEEDS"
        }
      }

    });

    lower_bound = res.next_key 
    
    more = res.more != "false" && res.more != ""

    console.log("payments: "+payments.length  + " next "+lower_bound)

  }



  totalCSVString = ""
  uniqueAccounts.forEach((account) => {
    item = totalsMap[account]
    totalCSVString = totalCSVString + account + "," + item.total + "," + item.totalSeeds + "\n"
  })


  fs.writeFileSync(snapshotDirPath + '03_payments_'+ timeStampString() +'.json', JSON.stringify(payments, null, 2))
  fs.writeFileSync(snapshotDirPath + '03_payments_'+ timeStampString() +'.csv', paymentsCSV)
  fs.writeFileSync(snapshotDirPath + '03_payments_totals_'+ timeStampString() +'.json', JSON.stringify(totalsMap, null, 2))
  fs.writeFileSync(snapshotDirPath + '03_payments_totals_'+ timeStampString() +'.csv', totalCSVString)

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


const getBalanceObjectFor = async (account) => {
  const balance = await getBalance(account)
  if (balance != null) {
    return{
      account: account,
      balance: balance,
      date: new Date().toISOString()
    }
  } else {
    return null
  }
}

const addBalances = async (balances, accounts) => {
  var futures = []
  accounts.forEach((acct) => futures.push(getBalanceObjectFor(acct)))
  var results = await Promise.all(futures)

  results.forEach(res => {
    if (res != null) {
      console.log("adding balance "+JSON.stringify(res, null, 2))
      balances.push(res)
    }
  });
  
}

const initDir = () => {
  if (!fs.existsSync(snapshotDir)) {
    fs.mkdirSync(snapshotDir)
  }
}

const getTokenHolders = async () => {
  var more = true
  var lower_bound = ''


  var accounts = []

  while (more) {
    const res = await get_table_by_scope({
      code: "token.seeds",
      table: "accounts",
      lower_bound: lower_bound,
      limit: 2000
    })
    
    //console.log("result: "+JSON.stringify(res, null, 2))
    
    if (res.rows && res.rows.length > 0) {
      var accts = res.rows.map((item) => item.scope)
      accts.forEach(item => { // JS concat driving me mad..
        accounts.push(item)
      });
    }
    lower_bound = res.more
    
    more = res.more != "false" && res.more != ""
    //more = false // debug

    console.log("accounts: "+accounts.length)

  }

  var balances = []
  var fileText = ""
  var errorAccounts = []

  var batchAccounts = []
  const batchSize = 100

  // var old = accounts
  // accounts = []
  // for(var i=0; i<200; i++) {
  //   accounts[i] = old[i]
  // }

  console.log("accts: "+accounts.length)

  for(var i = 0; i<accounts.length; i++) {
    const account = accounts[i]

    batchAccounts.push(account)

    if (i == accounts.length - 1 || batchAccounts.length == batchSize) {
      console.log("adding "+i+ " length "+batchAccounts.length)
      await addBalances(balances, batchAccounts)
      batchAccounts = []
    }
  }

  balances.forEach((b) => {
    fileText = fileText + b.account + "," +b.balance+ "," +b.date+ "\n" 
  })
      


  //console.log("balances: "+JSON.stringify(balances, null, 2))
  console.log("found "+accounts.length + " accounts" )

  fs.writeFileSync(snapshotDirPath + `seeds_errors_${timeStampString()}.json`, JSON.stringify(errorAccounts, null, 2))
  fs.writeFileSync(snapshotDirPath + `seeds_accounts_balances_${timeStampString()}.json`, JSON.stringify(balances, null, 2))
  fs.writeFileSync(snapshotDirPath + `seeds_accounts_balances_${timeStampString()}.csv`, fileText)
  
  //console.log("balances found: "+JSON.stringify(balances, null, 2))
  console.log("balances saved: "+balances.length)

}

program
  .command('balances')
  .description('Get SEEDS balances for all accounts')
  .action(async function () {
    console.log("getting balances");
    await getTokenHolders()
  })

program
  .command('planted')
  .description('Get Planted balances for all accounts')
  .action(async function () {
    console.log("getting planted");
    await allPlanted()
  })

program
  .command('payments')
  .description('Get all payments')
  .action(async function () {
    console.log("getting planted");
    await allPayments()
  })

program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}
 
