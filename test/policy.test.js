const { describe } = require('riteway')
const { eosNoNonce, names, isLocal, initContracts } = require('../scripts/helper')

const { policy, firstuser, seconduser } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

describe('policy', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const eos = eosNoNonce

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
    0,
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

  console.log("create expliring policy")

  const expiringPolicy = "this will expire in 1 second"

  await contract.createexp(
    accountField,
    secondDeviceUUID,
    secondDeviceField,
    signatureField,
    expiringPolicy,
    1,
    { authorization: `${firstuser}@active` }
  )

  const afterExpCreated = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'devicepolicy',
    json: true
  })

  const expTable = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'expiry',
    json: true
  })

  //console.log("afterExpCreated " + JSON.stringify(afterExpCreated, null, 2))
  //console.log("expTable " + JSON.stringify(expTable, null, 2))

  console.log("wait for expiry")
  await sleep(1000)

  const afterExpCreated2 = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'devicepolicy',
    json: true
  })

  const expTable2 = await eos.getTableRows({
    code: policy,
    scope: policy,
    table: 'expiry',
    json: true
  })

  //console.log("afterExpCreated2 " + JSON.stringify(afterExpCreated2, null, 2))
  //console.log("expTable2 " + JSON.stringify(expTable2, null, 2))


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

  assert({
    given: 'added expiring policy',
    should: 'have expiring policy',
    actual: afterExpCreated.rows[1],
    expected: {
      id: 2,
      account: accountField,
      backend_user_id: secondDeviceUUID,
      device_id: secondDeviceField,
      signature: signatureField,
      policy: expiringPolicy
    }
  })

  assert({
    given: 'added expiring policy',
    should: '2 policies',
    actual: afterExpCreated.rows.length,
    expected: 2
  })

  assert({
    given: 'added expiring policy after expiry date',
    should: 'policy is gone',
    actual: afterExpCreated2.rows.length,
    expected: 1
  })

  assert({
    given: 'added expiring policy',
    should: 'have expiry',
    actual: expTable.rows.length,
    expected: 1
  })

  assert({
    given: 'added expiring policy',
    should: 'have id entry in expiry table',
    actual: expTable.rows[0].id,
    expected: 2
  })

  assert({
    given: 'added expiring policy expiry table',
    should: 'have 1 second delay',
    actual: expTable.rows[0].valid_until - expTable.rows[0].created_at,
    expected: 1
  })

  assert({
    given: 'added expiring policy after expired',
    should: 'expiry info gone',
    actual: expTable2.rows.length,
    expected: 0
  })

})
