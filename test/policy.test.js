const { describe } = require('riteway')
const { eos, names, isLocal, initContracts } = require('../scripts/helper')

const { policy, firstuser, seconduser, accounts } = names

describe('policy', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  const contracts = await initContracts({ policy, accounts })

  let accountField = firstuser
  let uuidField = '123'
  let deviceField = '456'
  let signatureField = 'signature-string'
  let policyField = 'policy-string'

  await contracts.policy.reset({ authorization: `${policy}@active` })
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  await contracts.accounts.adduser(firstuser, 'First user', "individual", { authorization: `${accounts}@active` })

  await contracts.policy.create(
    accountField,
    uuidField,
    deviceField,
    signatureField,
    policyField,
    { authorization: `${firstuser}@active` }
  )
  
  policyField = 'updated-policy-string'

  await contracts.policy.update(
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

  await contracts.policy.create(
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

  await contracts.policy.remove(
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
    await contracts.policy.remove(
      0,
      { authorization: `${seconduser}@active` }
    )
    onlyAuthorizedCanRemove = false
  
  } catch (err) {
    console.log(err)

  }

  console.log("user who is not a seeds user tries to create");

  var nonSeedsUser = false
  var notSeedsErrorMessage = false
  try {
    await contracts.policy.create(
      seconduser,
      secondDeviceUUID,
      secondDeviceField,
      signatureField,
      policyField,
      { authorization: `${seconduser}@active` }
    )
      nonSeedsUser = true;
  } catch(e) {
    notSeedsErrorMessage = (""+e).toLowerCase().includes("not a seeds user")
    console.log(notSeedsErrorMessage +" expected error: "+e);
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

  assert({
    given: 'non seeds user',
    should: 'cannot create policy',
    actual: nonSeedsUser,
    expected: false
  })

  assert({
    given: 'non seeds user',
    should: 'error message',
    actual: notSeedsErrorMessage,
    expected: true
  })

})
