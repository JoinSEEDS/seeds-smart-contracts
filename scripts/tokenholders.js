#!/usr/bin/env node
const fetch = require("node-fetch");
const program = require('commander')
const fs = require('fs')

const host = "https://api.telosfoundation.io"

const { eos, getTableRows } = require("./helper");

const { migrateTokens } = require("./msig");

const { min } = require("ramda");
const { parse } = require("path");

const snapshotDir = "snapshots"
const snapshotDirPath = "snapshots/"

const getBalance = async ({
  user,
  code = 'token.seeds',
  symbol = 'SEEDS',
}) => {
  // const balance = await eos.getCurrencyBalance("token.seeds", user, 'SEEDS')

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
  var day = date.getDate()

  hours = hours < 10 ? '0'+hours : hours;
  minutes = minutes < 10 ? '0'+minutes : minutes;
  day = day < 10 ? '0'+day : day;
  month = month < 10 ? '0'+month : month;

  res = date.getFullYear() + month + day + hours + minutes;

  return res
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

  const get_history_tx = async (
    skip,
    limit,
) => {

  const url = host + `/v2/history/get_actions?skip=${skip}&limit=${limit}&act.account=token.hypha`

  const rawResponse = await fetch(url, {
      method: 'GET',
      headers: {
          'Accept': 'application/json',
          'Content-Type': 'application/json'
      },
      //body: JSON.stringify(params)
  });
  const res = await rawResponse.json();
  return res

}

const get_dao_history_tx = async (
  skip,
  limit,
) => {

const url = host + `/v2/history/get_actions?skip=${skip}&limit=${limit}&act.account=dao.hypha`

const rawResponse = await fetch(url, {
    method: 'GET',
    headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    },
    //body: JSON.stringify(params)
});
const res = await rawResponse.json();
return res

}

const getAllHistory = async () => {

  items = []
  skip = 0
  limit = 500
  hasMoreData = true

  transfers = ""

  while (hasMoreData) {

    console.log("skip: "+skip + " limit "+limit)

    newItems = await get_history_tx(skip, limit)
    newItems = newItems.actions
    //console.log("got items "+JSON.stringify(newItems, null, 2))
    newItems.forEach(item => {
      act = item.act
      data = act.data
      if (
        act.account == "token.hypha" &&
        act.name == "transfer" || act.name == "reduce"
        ) {
          if (act.name == "reduce") {
            console.log("adding reduce action "+JSON.stringify(item))
          }
          items.push(item)

          line = item.global_sequence + "," +item.timestamp + ","+
            data.from + "," + 
            data.to + "," + 
            data.amount + "," + 
            data.symbol + "," +
            data.quantity + "," +
            data.memo + "," +
            item.trx_id + "," 
          transfers = transfers + line + "\n";

          //console.log("line: "+line)
        }
    });
    skip = skip + newItems.length
    hasMoreData = newItems.length > 0


  }

  console.log("=========================================================================")
  console.log("all transfers: ")
  
  transfers = JSON.stringify(items, null, 2)

  console.log(transfers)

  //fs.writeFileSync(snapshotDirPath + `hypha_history_transfers.csv`, transfers)
  fs.writeFileSync(snapshotDirPath + `hypha_history_transfers.json`, transfers)

}

const getAllHistorySinceReset = async () => {

  items = []
  skip = 0
  limit = 400
  hasMoreData = true


  const cutoff_sequence_number = 9801481669

  transfers = ""

  var resetDateReached = false

  while (hasMoreData && !resetDateReached) {

    //console.log("skip: "+skip + " limit "+limit)

    newItems = await get_history_tx(skip, limit)
    newItems = newItems.actions
    //console.log("got items "+JSON.stringify(newItems, null, 2))

    for (item of newItems) {
      act = item.act
      data = act.data
      
      console.log("item " + item.global_sequence + " " + (item.global_sequence >= cutoff_sequence_number))

      console.log(JSON.stringify(item, null, 2))

      console.log("")

      if (item.global_sequence >= cutoff_sequence_number)     // first costak transaction
      {
        items.push(item)

          console.log("adding "+ item.global_sequence)

          line = item.global_sequence + "," +item.timestamp + ","+
            data.from + "," + 
            data.to + "," + 
            data.amount + "," + 
            data.symbol + "," +
            data.quantity + "," +
            data.memo + "," +
            item.trx_id + "," 
          transfers = transfers + line + "\n";

          //console.log("line: "+line)
        } else {
          console.log("Done! Reset started at global action sequence " + item.global_sequence + " " + cutoff_sequence_number)
          resetDateReached = true
        }
    }
    skip = skip + newItems.length
    hasMoreData = newItems.length > 0
    

  }

  console.log("=========================================================================")
  //console.log("all transfers: ")
  //console.log(transfers)

  //console.log("transfer JSON " + items.length)
  //console.log(JSON.stringify(items, null, 2))

  fs.writeFileSync(snapshotDirPath + `hypha_history_since_reset.csv`, transfers)
  fs.writeFileSync(snapshotDirPath + `hypha_history_since_reset.json`, JSON.stringify(items, null, 2))

}

const process_transfers = async (full = false)=> {

try {
  const fileContent = !full 
    ? fs.readFileSync(snapshotDirPath + `hypha_history_since_reset.json`, 'utf8')
    : fs.readFileSync(snapshotDirPath + `hypha_history_transfers.json`, 'utf8')

  console.log("Process history " + (full ? "FULL" : "Since reset"))

  const items = JSON.parse(fileContent)

  //console.log(JSON.stringify(items, null, 2))

  var balances = {
    illumination: 0.0
  }

  const bal = (account) => {
    return balances[account] ?? 0
  }

  for (item of items) {
    if (item.act.name == "transfer") {
      const data = item.act.data
      const to = data.to
      const from = data.from
      const amount = data.amount
      const symbol = data.symbol
  
  
      if (symbol != "HYPHA") {
        console.log("not hypha error "+JSON.stringify(item, null, 2))
        // NOTE: Some HUSD were on token.hypha at some point
        continue
      }
      console.log("from "+ from + " to: "+to + " " + amount + " " + symbol + " "+bal(to))
  
      balances[to] = bal(to) + amount
      balances[from] = bal(from) - amount

    } else if (item.act.name == "reduce") {

      const data = item.act.data
      const account = data.account
      const amount = parseFloat(data.quantity.split(" ")[0])
      const symbol = data.quantity.split(" ")[1]
  
      if (symbol != "HYPHA") {
        console.log("not hypha error "+JSON.stringify(item))
        throw "Error: not hypha"
        
      }
      console.log("reduce: "+account + " " + amount)
  
      balances[account] = bal(account) - amount

    }

    
  }

  var keys = Object.keys(balances);
  keys = keys.sort()
  var balancesAsc = {}
  transferlist = []
  for (key of keys) {
    balancesAsc[key] = balances[key]
    if (balances[key] >= 0.01) {
      transferlist.push({
        account: key,
        amount: balances[key],
      })  
    }
  }

  //console.log(mapAsc)

  console.log("balances: "+JSON.stringify(balancesAsc, null, 2))
  console.log("transferlist: "+JSON.stringify(transferlist, null, 2))

  var csv = ""

  for (item of transferlist) {
    csv = csv + `${item.account},${item.amount.toFixed(2)}` + "\n"
  }

  console.log("balance csv \n" + csv)
  
} catch (err) {
  console.log(err)
}

}

const get_tlos_history = async (
  account,
  skip,
  limit,
) => {

const url = `https://api.telosfoundation.io/v2/history/get_actions?account=${account}&act.account=eosio.token&act.name=transfer&skip=${skip}&limit=${limit}&sort=desc`

const rawResponse = await fetch(url, {
    method: 'GET',
    headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    },
    //body: JSON.stringify(params)
});
const res = await rawResponse.json();
return res

}

const getTelosHistory = async (account, minimum = 0) => {

  items = []
  skip = 0
  limit = 400
  hasMoreData = true

  transfers = ""

  while (hasMoreData) {

    console.log("skip: "+skip + " limit "+limit)

    newItems = await get_tlos_history(account, skip, limit)
    newItems = newItems.actions
    //console.log("got items "+JSON.stringify(newItems, null, 2))
    newItems.forEach(item => {
      items.push(item)
      const act = item.act
      const data = act.data
      if (
        act.account == "eosio.token" &&
        act.name == "transfer" &&
        data.from == account) {
          line = 
            item.global_sequence + "," +
            item.timestamp + ","+
            data.from + "," + 
            data.to + "," + 
            data.amount + "," + 
            data.symbol + "," +
            data.quantity + "," +
            data.memo + "," +
            item.trx_id + "," 
          transfers = transfers + line + "\n";

          var txid = data.amount > 2.5 ? item.trx_id : ""

          console.log(data.quantity + " " + data.from + " ==> "+ data.to + " " + item.timestamp + " " + data.memo + " " + txid)

          //console.log("line: "+line)
        }
    });
    skip = skip + newItems.length
    hasMoreData = newItems.length > 0


  }

  console.log("=========================================================================")
  console.log("all transfers: ")
  console.log(transfers)

  fs.writeFileSync(snapshotDirPath + `telos_history_transfers.csv`, transfers)

}

const getDAOHistory = async () => {

  items = []
  skip = 0
  limit = 100
  hasMoreData = true

  transfers = ""

  while (hasMoreData) {

    console.log("skip: "+skip + " limit "+limit)

    newItems = await get_dao_history_tx(skip, limit)
    newItems = newItems.actions
    console.log("got items "+JSON.stringify(newItems, null, 2))
    newItems.forEach(item => {
      items.push(item)
      act = item.act
      data = act.data
      if (
        act.account == "dao.hypha" &&
        act.name == "transfer" &&
        data.from == "dao.hypha") {
          line = item.global_sequence + "," +item.timestamp + ","+
            data.from + "," + 
            data.to + "," + 
            data.amount + "," + 
            data.symbol + "," +
            data.quantity + "," +
            data.memo + "," +
            item.trx_id + "," 
          transfers = transfers + line + "\n";

          console.log("line: "+line)
        }
    });
    skip = skip + newItems.length
    hasMoreData = newItems.length > 0


  }

  console.log("=========================================================================")
  console.log("all transfers: ")
  console.log(transfers)

  fs.writeFileSync(snapshotDirPath + `hypha_history_transfers.csv`, transfers)

}


const getBalanceObjectFor = async (account, code, symbol) => {
  const balance = await getBalance({
    user: account,
    code: code,
    symbol: symbol
  })
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

const addBalances = async (balances, accounts, contract, symbol) => {
  var futures = []
  accounts.forEach((acct) => futures.push(getBalanceObjectFor(acct, contract, symbol)))
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
      await addBalances(balances, batchAccounts, "token.seeds", "SEEDS")
      batchAccounts = []
    }
  }

  balances.sort((a, b) => parseFloat(a.balance) - parseFloat(b.balance))

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

const getAnyTokenHolders = async ({
  contract = "token.seeds",
  symbol = "SEEDS",
  prefix = "seeds_"
}) => {
  var more = true
  var lower_bound = ''


  var accounts = []

  while (more) {
    const res = await get_table_by_scope({
      code: contract,
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

    console.log(prefix + "accounts: "+accounts.length)

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
      await addBalances(balances, batchAccounts, contract, symbol)
      batchAccounts = []
    }
  }

  // sort high to low descending
  balances.sort((a, b) => parseFloat(b.balance) - parseFloat(a.balance))

  total = 0.0
  balances.forEach((b) => {
    total += parseFloat(b.balance)
  })


  fileText = "account,amount,%,balance,date\n"

  balances.forEach((b) => {
    percent = (parseFloat(b.balance) * 100 / total).toFixed(2)
    fileText = fileText + b.account + "," +parseFloat(b.balance) + ',' + percent + "," +b.balance+ "," +b.date+ "\n" 
  })
      


  //console.log("balances: "+JSON.stringify(balances, null, 2))
  console.log("found "+accounts.length + " accounts" )

  fs.writeFileSync(snapshotDirPath + prefix + `errors_${timeStampString()}.json`, JSON.stringify(errorAccounts, null, 2))
  fs.writeFileSync(snapshotDirPath + prefix + `accounts_balances_${timeStampString()}.json`, JSON.stringify(balances, null, 2))
  fs.writeFileSync(snapshotDirPath + prefix + `accounts_balances_${timeStampString()}.csv`, fileText)
  
  //console.log("balances found: "+JSON.stringify(balances, null, 2))
  console.log(prefix +"balances saved: "+balances.length)

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

program
  .command('time')
  .action(async function () {
    console.log("getting time");
    await timeStampString()
  })

program
  .command('hvoice')
  .description('Get HVOICE balances for all accounts')
  .action(async function () {
    console.log("getting HVOICE balances");
    await getAnyTokenHolders({
      contract: "voice.hypha",
      symbol: "HVOICE",
      prefix: "HVOICE_"
    })
  })

  program
  .command('hypha')
  .description('Get HYPHA balances for all accounts')
  .action(async function () {
    console.log("getting HYPHA balances");
    await getAnyTokenHolders({
      contract: "token.hypha",
      symbol: "HYPHA",
      prefix: "HYPHA_"
    })
  })

  program
  .command('husd')
  .description('Get HUSD balances for all accounts')
  .action(async function () {
    console.log("getting HUSD balances");
    await getAnyTokenHolders({
      contract: "husd.hypha",
      symbol: "HUSD",
      prefix: "HUSD_"
    })
  })

  program
  .command('history')
  .description('Get HYPHA token history')
  .action(async function () {
    console.log("getting HYPHA token history");
    await getAllHistory()
  })

  program
  .command('history_since_reset')
  .description('Get HYPHA token history since HYPHA Reset 2022/01/15 ')
  .action(async function () {
    console.log("getting HYPHA token history since reset");
    await getAllHistorySinceReset()
  })

  program
  .command('process_history [full]')
  .description('Process HYPHA token history since HYPHA Reset 2022/01/15 ')
  .action(async function (full = false) {
    console.log("Process HYPHA token history "+(full ? "full" : "since HYPHA Reset 2022/01/15"));
    await process_transfers(full)
  })

program
  .command('dao')
  .description('Get DAO history')
  .action(async function () {
    console.log("getting DAO history");
    await getDAOHistory()
  })

program
  .command('migrate_tokens')
  .description('Migrate tokens ESR')
  .action(async function () {

    console.log("migrate tokens")

    const list = [
      {
        "account": "bigorna12345",
        "amount": 1.1700000000000002
      },
      {
        "account": "blockchain5d",
        "amount": 1.6400000000000001
      },
      {
        "account": "buy.hypha",
        "amount": 899059.04
      },
      {
        "account": "costak.hypha",
        "amount": 42971363.42
      },
      {
        "account": "dangermouse1",
        "amount": 0.1
      },
      {
        "account": "illumination",
        "amount": 3.3899999999999997
      },
      {
        "account": "leonieherma1",
        "amount": 10
      },
      {
        "account": "markflowfarm",
        "amount": 827.89
      },
      {
        "account": "mindmonkey12",
        "amount": 100.58
      },
      {
        "account": "nadimhamdan1",
        "amount": 5.59
      },
      {
        "account": "pedroteux123",
        "amount": 0.09
      },
      {
        "account": "rpiesveloces",
        "amount": 41.03
      }
    ]
    
    
    await migrateTokens(list)
  })

program
  .command('telos <account>')
  .description('Get TLOS token history')
  .action(async function (account) {
    console.log("getting TLOS token history");
    await getTelosHistory(account)
  })


program.parse(process.argv)

var NO_COMMAND_SPECIFIED = program.args.length === 0;
if (NO_COMMAND_SPECIFIED) {
  program.help();
}
 
