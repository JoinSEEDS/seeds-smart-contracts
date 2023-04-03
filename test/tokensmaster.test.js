const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, initContracts, sleep, asset, getBalanceFloat } = require("../scripts/helper")

const { firstuser, seconduser, thirduser, fourthuser, token, tokensmaster } = names


describe('Master token list', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  console.log('installed at '+tokensmaster)
  const contract = await eos.contract(tokensmaster)
  const thetoken = await eos.contract(token)

  console.log('--Normal operations--')
  console.log('reset')
  await contract.reset({ authorization: `${tokensmaster}@active` })

  //return;

  console.log('init')


  await contract.init('Telos', seconduser, true, { authorization: `${tokensmaster}@active` })

  const getConfigTable = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'config',
      json: true,
    })
  }

  function dropTime(object) { object.init_time = ""; return object };

  const getWhitelist = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'whitelist',
      json: true,
    })
  }

  const getBlacklist = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'blacklist',
      json: true,
    })
  }
 
  assert({
    given: 'initializing contract',
    should: 'create config table, whitelist, blacklist',
    actual: [ dropTime( (await getConfigTable())['rows'][0] ),
              (await getWhitelist())['rows'],
              (await getBlacklist())['rows'] ],
    expected: [ { chain: "Telos", manager: "seedsuserbbb",
                  verify: 1, init_time: "" },
                [ {"id":0,"chainName":"Telos","token":{"sym":"4,SEEDS","contract":"token.seeds"}},
                  {"id":1,"chainName":"Telos","token":{"sym":"4,TLOS","contract":"eosio.token"}} ],
                [ {"sym_code":"BTC"}, {"sym_code":"ETH"}, {"sym_code":"TLOS"}, {"sym_code":"SEEDS"} ]
              ]
  })

  console.log('submit token')

  const testToken = token
  const testSymbol = 'SEEDS'

  await contract.submittoken(thirduser, 'Telos', testToken, testSymbol,
    '{"name": "Seeds token", "logo": "somelogo", "precision": "6", "baltitle": "Wallet balance", '+
     '"baltitle_es": "saldo de la billetera", "backgdimage": "someimg"}',
    { authorization: `${thirduser}@active` })

  const getTokenTable = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'tokens',
      json: true,
    })
  }

  assert({
    given: 'submitting token',
    should: 'create token entry',
    actual: (await getTokenTable())['rows'],
    expected: [ {id:0, submitter:"seedsuserccc",chainName:"Telos",contract:"token.seeds",symbolcode:"SEEDS",
                 json:"{\"name\": \"Seeds token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"baltitle\": \"Wallet balance\", \"baltitle_es\": \"saldo de la billetera\", \"backgdimage\": \"someimg\"}"} ]
  })

  console.log('accept token')

  await contract.accepttoken(0, testSymbol, 'usecase1', true,
    { authorization: `${seconduser}@active` })

  const getUsecaseTable = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'usecases',
      json: true,
    })
  }

  const getAcceptanceTable = async (usecase) => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: usecase,
      table: 'acceptances',
      json: true,
    })
  }


  assert({
    given: 'accepting token',
    should: 'accept token entry',
    actual: [(await getUsecaseTable())['rows'], (await getAcceptanceTable('usecase1'))['rows'] ],
    expected: [ [{"usecase":"usecase1", "curator":"seedsuserbbb"} ], [{"token_id":0}] ]
  })

  console.log('add second token')
  const secondSymbol = 'TESTS'

  await contract.submittoken(fourthuser, 'Telos', testToken, secondSymbol,
    '{"name": "Test token", "logo": "somelogo", "precision": "6", "baltitle": "Wallet balance", '+
     '"baltitle_es": "saldo de la billetera", "backgdimage": "someimg"}',
    { authorization: `${fourthuser}@active` })
  await contract.accepttoken(1, secondSymbol, 'usecase1', true,
    { authorization: `${seconduser}@active` })

  assert({
    given: 'add token',
    should: 'approve token entry',
    actual: (await getTokenTable())['rows'],
    expected: [ {id:0, submitter:"seedsuserccc",chainName:"Telos",contract:"token.seeds",symbolcode:"SEEDS",
                 json:"{\"name\": \"Seeds token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"baltitle\": \"Wallet balance\", \"baltitle_es\": \"saldo de la billetera\", \"backgdimage\": \"someimg\"}"},
                {id:1,submitter:"seedsuserxxx",chainName:"Telos",contract:"token.seeds",symbolcode:"TESTS",
                 json:"{\"name\": \"Test token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"baltitle\": \"Wallet balance\", \"baltitle_es\": \"saldo de la billetera\", \"backgdimage\": \"someimg\"}"} ]
  })

  console.log('change manager/curator')

  await contract.setcurator('', thirduser,
    { authorization: `${seconduser}@active` })
  await contract.setcurator('usecase1', fourthuser,
    { authorization: `${thirduser}@active` })

  assert({
    given: 'change manager and usecase1 curator',
    should: 'change them',
    actual: [(await getConfigTable())['rows'][0]['manager'], (await getUsecaseTable())['rows'] ],
    expected: [ "seedsuserccc", [{"usecase":"usecase1", "curator":"seedsuserxxx"} ] ]
  })

  console.log('unaccept token')

  await contract.accepttoken(0, testSymbol, 'usecase1', false,
    { authorization: `${thirduser}@active` })

  assert({
    given: 'unaccept token',
    should: 'remove token acceptance',
    actual: (await getAcceptanceTable('usecase1'))['rows'],
    expected: [ {"token_id":1} ]
  })

  console.log('token delete')

  await contract.deletetoken(0, testSymbol, { authorization: `${thirduser}@active` })

  assert({
    given: 'deleting token',

    should: 'delete token',
    actual: [(await getUsecaseTable())['rows'], (await getTokenTable('usecase1'))['rows']],
    expected: [
       [ {"usecase":"usecase1", "curator":"seedsuserxxx"} ],
       [ {id:1,submitter:"seedsuserxxx",chainName:"Telos",contract:"token.seeds",symbolcode:"TESTS",
                 json:"{\"name\": \"Test token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"baltitle\": \"Wallet balance\", \"baltitle_es\": \"saldo de la billetera\", \"backgdimage\": \"someimg\"}"}]
      ]
  })

  console.log('modify whitelist & blacklist')

  await contract.updwhitelist( "Telos", {contract: "rainbo.seeds", sym: "3,PIGS" }, true,
        { authorization: `${thirduser}@active` })
  await contract.updwhitelist( "Telos", {contract: "token.seeds", sym: "4,SEEDS" }, false,
        { authorization: `${thirduser}@active` })

  await contract.updblacklist("BLACKS", true, { authorization: `${thirduser}@active` })
  await contract.updblacklist("BTC", false, { authorization: `${thirduser}@active` })

  assert({
    given: 'changing white/blacklists',

    should: 'see lists changed',
    actual: [ (await getWhitelist())['rows'], (await getBlacklist())['rows'] ],
    expected: [
       [ {"id":1,"chainName":"Telos","token":{"sym":"4,TLOS","contract":"eosio.token"}},
         {"id":2,"chainName":"Telos","token":{"sym":"3,PIGS","contract":"rainbo.seeds"}} ],
       [ {"sym_code":"ETH"}, {"sym_code":"TLOS"}, {"sym_code":"SEEDS"} , {"sym_code":"BLACKS"}]
      ]
  })


  console.log('Failing operations')

  console.log('TBD')


})
