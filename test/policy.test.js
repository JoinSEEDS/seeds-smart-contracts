const { describe } = require('riteway')
const { eos, names } = require('../scripts/helper')

const { policy, firstuser } = names

  describe('policy', async assert => {
  const contract = await eos.contract(policy)

  console.log('reset tables')
  await contract.reset({ authorization: `${policy}@active`})

  let accountField = firstuser
  let uuidField = '123'
  let signatureField = 'signature-string'
  let policyField = 'policy-string'

  await contract.create(
    accountField,
    uuidField,
    signatureField,
    policyField,
    { authorization: `${account}@active` }
  )

  policyField = 'updated-policy-string'

  await contract.update(
    accountField,
    uuidField,
    signatureField,
    policyField,
    { authorization: `${account}@active` }
  )

  const { rows } = await eos.getTableRows({
    code: policy,
    scope: account,
    table: 'policies',
    json: true
  })

  assert({
    given: 'created & updated policy',
    should: 'show created row in table',
    actual: rows,
    expected: [{
      accountField,
      uuidField,
      signatureField,
      policyField
    }]
  })
})
