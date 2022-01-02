const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, initContracts, sleep, asset, getBalanceFloat } = require("../scripts/helper")

const { firstuser, seconduser, thirduser, token, tokensmaster } = names


describe('Master token list', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(tokensmaster)
  const thetoken = await eos.contract(token)

  const users = [firstuser, seconduser, thirduser]

  console.log('reset')
  await contract.reset({ authorization: `${tokensmaster}@active` })

  console.log('usecase create')

  await contract.usecase('usecase1', firstuser, true, { authorization: `${tokensmaster}@active` })
  await contract.usecase('usecase2', seconduser, true, { authorization: `${tokensmaster}@active` })
  await contract.usecase('usecase3', seconduser, true, { authorization: `${tokensmaster}@active` })

  const getUsecaseTable = async () => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: tokensmaster,
      table: 'usecases',
      json: true,
    })
  }

  assert({
    given: 'creating usecases',
    should: 'create usecase entries',
    actual: (await getUsecaseTable())['rows'],
    expected: [ { usecase: 'usecase1', manager: firstuser, unique_symbols: 0, allowed_chain: '', required_fields: '' },
                { usecase: 'usecase2', manager: seconduser, unique_symbols :0, allowed_chain: '', required_fields: '' },
                { usecase: 'usecase3', manager: seconduser, unique_symbols :0, allowed_chain: '', required_fields: '' } ]
  })

  console.log('usecase config')

  await contract.usecasecfg('usecase1', true, 'EOS', 'name logo balance.es weblink',
                            { authorization: `${firstuser}@active` })
  await contract.usecasecfg('usecase2', true, 'Telos', 'name logo weblink',
                            { authorization: `${seconduser}@active` })
  await contract.usecasecfg('usecase1', false, 'Telos', 'name backgdimage logo balancesubt precision',
                            { authorization: `${firstuser}@active` })


  assert({
    given: 'configuring usecases',
    should: 'update usecase entries',
    actual: (await getUsecaseTable())['rows'],
    expected: [ { usecase: 'usecase1', manager: firstuser, unique_symbols: 0, allowed_chain: 'Telos', required_fields: 'name backgdimage logo balancesubt precision' },
                { usecase: 'usecase2', manager: seconduser, unique_symbols :1, allowed_chain: 'Telos', required_fields: 'name logo weblink' },
                { usecase: 'usecase3', manager: seconduser, unique_symbols :0, allowed_chain: '', required_fields: '' } ]
  })

  console.log('usecase delete')

  await contract.usecase('usecase3', seconduser, false, { authorization: `${tokensmaster}@active` })

  assert({
    given: 'deleting usecase',

    should: 'delete usecase entry',
    actual: (await getUsecaseTable())['rows'],
    expected: [ { usecase: 'usecase1', manager: firstuser, unique_symbols: 0, allowed_chain: 'Telos', required_fields: 'name backgdimage logo balancesubt precision' },
                { usecase: 'usecase2', manager: seconduser, unique_symbols :1, allowed_chain: 'Telos', required_fields: 'name logo weblink' } ]
  })

  console.log('submit token')

  const testToken = token
  const testSymbol = 'SEEDS'

  await contract.submittoken(thirduser, 'usecase1', 'Telos', testToken, testSymbol,
    '{"name": "Seeds token", "logo": "somelogo", "precision": "6", "balancesubt": "Wallet balance", "backgdimage": "someimg"}',
    { authorization: `${thirduser}@active` })

  const getTokenTable = async (usecase) => {
    return await eos.getTableRows({
      code: tokensmaster,
      scope: usecase,
      table: 'tokens',
      json: true,
    })
  }

  assert({
    given: 'submitting token',
    should: 'create token entry',
    actual: (await getTokenTable('usecase1'))['rows'],
    expected: [ {id:0, submitter:"seedsuserccc",usecase:"usecase1",chainName:"Telos",contract:"token.seeds",symbolcode:"SEEDS",
                 approved:0,json:"{\"name\": \"Seeds token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"balancesubt\": \"Wallet balance\", \"backgdimage\": \"someimg\"}"} ]
  })

  console.log('approve token')

  await contract.approvetoken(thirduser, 'usecase1', 'Telos', testToken, testSymbol, true,
    { authorization: `${firstuser}@active` })


  assert({
    given: 'approving token',
    should: 'approve token entry',
    actual: (await getTokenTable('usecase1'))['rows'],
    expected: [ {id:0, submitter:"seedsuserccc",usecase:"usecase1",chainName:"Telos",contract:"token.seeds",symbolcode:"SEEDS",
                 approved:1,json:"{\"name\": \"Seeds token\", \"logo\": \"somelogo\", \"precision\": \"6\", \"balancesubt\": \"Wallet balance\", \"backgdimage\": \"someimg\"}"} ]
  })


})
