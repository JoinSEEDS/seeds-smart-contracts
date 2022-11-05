const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, sha256, fromHexString, isLocal, ramdom64ByteHexString, createKeypair, getBalance, sleep } = require('../scripts/helper')

const { joinhypha, firstuser, seconduser } = names

const randomAccountName = () => {
  let length = 12
  var result = '';
  var characters = 'abcdefghijklmnopqrstuvwxyz1234';
  var charactersLength = characters.length;
  for (var i = 0; i < length; i++) {
      result += characters.charAt(Math.floor(Math.random() * charactersLength));
  }
  return result;
}

describe('create account', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const newAccount = randomAccountName()
  console.log("New account " + newAccount)
  const keyPair = await createKeypair()
  console.log("new account keys: " + JSON.stringify(keyPair, null, 2))
  const newAccountPublicKey = keyPair.public

  const contract = await eos.contract(joinhypha)


  console.log("set config")
  // ACTION setconfig ( const name& account_creator_contract, const name& account_creator_oracle );
  await contract.setconfig( seconduser, seconduser, { authorization: `${joinhypha}@active` })

  console.log("activate")
  await contract.activate( { authorization: `${joinhypha}@active` })

  console.log("create")
  // ACTION create ( const name& account_to_create, const string& key);
  await contract.create(newAccount, newAccountPublicKey, { authorization: `${seconduser}@active` })
  
  var anybodyCanCreateAnAccount = false
  try {
    const acct2 = randomAccountName()
    console.log("creating acct "+acct2)
    await contract.create(acct2, newAccountPublicKey, { authorization: `${firstuser}@active` })
    anybodyCanCreateAnAccount = true;
  } catch (err) {
    console.log("expected error")
  }

  const config = await eos.getTableRows({
    code: joinhypha,
    scope: joinhypha,
    table: 'config',
    json: true
  })

  console.log("conig "+ JSON.stringify(config))

  const account = await eos.getAccount(newAccount)

  console.log("new account exists: "+JSON.stringify(account))

  assert({
    given: 'create account',
    should: 'a new account has been created on the blockchain',
    actual: account.account_name,
    expected: newAccount
  })

  assert({
    given: 'create account by invalid oracle',
    should: 'oracle cant create accounts',
    actual: anybodyCanCreateAnAccount,
    expected: false
  })



})
