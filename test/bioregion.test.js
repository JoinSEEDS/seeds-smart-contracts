const { describe } = require("riteway")
const { eos, encodeName, names, getTableRows, isLocal, initContracts } = require("../scripts/helper")

const { accounts, harvest, bioregion, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser } = names

describe("Bioregions General", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, bioregion, token, settings })

  console.log('bioregion reset')
  await contracts.bioregion.reset({ authorization: `${harvest}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('configure')
  await contracts.settings.configure("hrvstreward", 10000 * 100, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })

  assert({
    given: 'bioregions rock ',
    should: 'rock',
    actual: "yes",
    expected: "yes"
  })

})
