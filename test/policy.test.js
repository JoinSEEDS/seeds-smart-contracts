const { describe } = require('riteway')
const { eos, names, isLocal } = require('../scripts/helper')

const { policy, firstuser } = names

describe('policy', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contract = await eos.contract(policy)

  let accountField = firstuser
  let uuidField = '123'
  let deviceField = '456'
  let signatureField = 'signature-string'
  let policyField = 'policy-string'

  await contract.reset({ authorization: `${policy}@active` })

  await contract.create(
    accountField,
    uuidField,
    deviceField,
    signatureField,
    policyField,
    { authorization: `${firstuser}@active` }
  )
  
  policyField = 'updated-policy-string'

  await contract.update(
    accountField,
    uuidField,
    deviceField,
    signatureField,
    policyField,
    { authorization: `${firstuser}@active` }
  )

  const { rows } = await eos.getTableRows({
    code: policy,
    scope: firstuser,
    table: 'policiesnew',
    json: true
  })
  
  assert({
    given: 'created & updated policy',
    should: 'show created row in table',
    actual: rows,
    expected: [{
      account: accountField,
      backend_user_id: uuidField,
      device_id: deviceField,
      signature: signatureField,
      policy: policyField
    }]
  })
})
