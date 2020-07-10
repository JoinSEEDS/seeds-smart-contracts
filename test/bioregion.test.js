const { describe } = require("riteway")
const { eos, encodeName, names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")

const { accounts, harvest, bioregion, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser } = names

describe("Bioregions General", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, bioregion, token, settings })

  const keypair = await createKeypair();

  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('bioregion reset')
  await contracts.bioregion.reset({ authorization: `${bioregion}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('configure - 1 Seeds feee')
  await contracts.settings.configure("bio.fee", 10000 * 1, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'second user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('transfer fee for a bioregion')
  await contracts.token.transfer(firstuser, bioregion, "1.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })

  const balances = await getTableRows({
    code: bioregion,
    scope: bioregion,
    table: 'sponsors',
    json: true
})

// ACTION create(
//   name founder, 
//   name bioaccount, 
//   string description, 
//   string locationJson, 
//   float latitude, 
//   float longitude, 
//   string publicKey);

  console.log('create a bioregion')
  await contracts.bioregion.create(
    firstuser, 
    'testbr.bdc', 
    'test bio region',
    '{lat:0.0111,lon:1.3232}', 
    1.1, 
    1.23, 
    keypair.public, 
    { authorization: `${firstuser}@active` })

  console.log('get refund from bioregion')


  assert({
    given: 'bioregions rock ',
    should: 'rock',
    actual: "yes",
    expected: "yes"
  })

})
