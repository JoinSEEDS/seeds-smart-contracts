const { describe } = require('riteway')
const { eos, names, isLocal } = require('../scripts/helper')

const { settings, firstuser } = names

describe('settings', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(settings)

  await contract.reset({ authorization: `${settings}@active` })

  const { rows: config_rows } = await eos.getTableRows({
    code: settings,
    scope: settings,
    table: 'config',
    limit: 1000,
    json: true
  })

  const { rows: contract_rows } = await eos.getTableRows({
    code: settings,
    scope: settings,
    table: 'contracts',
    limit: 1000,
    json: true
  })

  let params = config_rows.map(({ param }) => param)
  //console.log("params "+JSON.stringify(params, 0, 2))

  let accounts = contract_rows.map(({ account }) => account)
  //console.log("accounts "+JSON.stringify(accounts, 0, 2))

  assert({
    given: 'reset settings',
    should: 'have config items with params',
    actual: params.length > 0,
    expected: true
  })

  assert({
    given: 'reset settings',
    should: 'have contract items with accounts',
    actual: accounts.length > 0,
    expected: true
  })

  await contract.configure("testing", 77, { authorization: `${settings}@active` })

  const afterSetting = await eos.getTableRows({
    code: settings,
    scope: settings,
    lower_bound: 'testing',
    upper_bound: 'testing',
    table: 'config',
    limit: 1,
    json: true
  })

  assert({
    given: 'set a value',
    should: 'have value',
    actual: afterSetting.rows.filter( ({param}) => param == "testing")[0].value,
    expected: 77
  })

  await contract.remove("testing", { authorization: `${settings}@active` })

  const afterSettingRemoved = await eos.getTableRows({
    code: settings,
    scope: settings,
    lower_bound: 'testing',
    upper_bound: 'testing',
    table: 'config',
    limit: 1,
    json: true
  })

  assert({
    given: 'remove a value',
    should: 'value has been deleted',
    actual: afterSettingRemoved.rows,
    expected: []
  })


})
