const Eos = require('eosjs')
const R = require('ramda')

const networks = {
  mainnet: 'aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906',
  jungle: 'e70aaab8997e1dfce58fbfac80cbbb8fecec7b99cf982a9444273cbc64c41473',
  kylin: '5fff1dae8dc8e2fc4d5b23b2c7665c97f9e9d8edf2b6485a86ba311c25639191',
  local: 'cf057bbfb72640471fd910bcb67639c22df9f92470936cddc1ade0e2f2e7dc4f'
}

const account = (name) => ({
  type: 'account',
  account: name,
  creator: 'owner',
  publicKey: 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV',
  stakes: {
    cpu: '5.0000 EOS',
    net: '5.0000 EOS',
    ram: 8096
  }
})

const contract = (name) => ({
  ...account(name),
  type: 'contract',
  name
})

const accountsMetadata = (network) => {
  if (network !== networks.local)
    throw new Error(`${network} is not supported network`)

  return {
    firstuser: account('firstuser'),
    seconduser: account('seconduser'),
    application: account('application'),
    bank: account('bank'),
    accounts: contract('accounts'),
    harvest: contract('harvest'),
    subscription: contract('subscription'),
    token: {
      ...contract('token'),
      name: 'eosio.token',
      issuer: 'owner',
      supply: '1000000.0000 EOS',
    }
  }
}

const {
  EOSIO_NETWORK,
  EOSIO_API_ENDPOINT,
  EOSIO_CHAIN_ID,
} = process.env

const chainId = EOSIO_CHAIN_ID || networks[EOSIO_NETWORK] || networks.local

const accounts = accountsMetadata(chainId)
const names = R.mapObjIndexed((item) => item.type == 'account' ? item.account : item.name, accounts)

const config = {
  keyProvider: '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3',
  httpEndpoint: process.env.EOSIO_API_ENDPOINT || 'http://0.0.0.0:8888',
  chainId
}

const eos = Eos(config)

const encodeName = Eos.modules.format.encodeName

const getBalance = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  return Number.parseInt(balance[0])
}

module.exports = {
  eos, encodeName, getBalance,
  accounts, names
}
