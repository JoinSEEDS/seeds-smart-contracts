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
    json: true
  })

  const { rows: contract_rows } = await eos.getTableRows({
    code: settings,
    scope: settings,
    table: 'contracts',
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
})
