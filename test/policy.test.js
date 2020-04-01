const { describe } = require('riteway')
const { eos, names, isLocal } = require('../scripts/helper')

const { policy, firstuser, seconduser } = names

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
    0,
    accountField,
    uuidField,
    deviceField,
    signatureField,
    policyField,
    { authorization: `${firstuser}@active` }
  )

  const { rows } = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'devicepolicy',
    json: true
  })

  let secondDeviceUUID = '777-999'
  let secondDeviceField = '13:33'

  await contract.create(
    accountField,
    secondDeviceUUID,
    secondDeviceField,
    signatureField,
    policyField,
    { authorization: `${firstuser}@active` }
  )

  const afterAddSecondDevice = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'devicepolicy',
    json: true
  })

  await contract.remove(
    1,
    { authorization: `${firstuser}@active` }
  )

  const afterRemove = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'devicepolicy',
    json: true
  })

  var onlyAuthorizedCanRemove = true
  try {
    await contract.remove(
      0,
      { authorization: `${seconduser}@active` }
    )
    onlyAuthorizedCanRemove = false
  
  } catch (err) {
    console.log(err)

  }


  assert({
    given: 'added second policy',
    should: 'have 2 policies',
    actual: afterAddSecondDevice.rows[1],
    expected: {
      id: 1,
      account: accountField,
      backend_user_id: secondDeviceUUID,
      device_id: secondDeviceField,
      signature: signatureField,
      policy: policyField
    }
  })

  assert({
    given: 'created & updated policy',
    should: 'show created row in table',
    actual: rows,
    expected: [{
      id: 0,
      account: accountField,
      backend_user_id: uuidField,
      device_id: deviceField,
      signature: signatureField,
      policy: policyField
    }]
  })

  assert({
    given: 'deleted policy',
    should: 'row deleted',
    actual: afterRemove.rows.length,
    expected: 1
  })

})
