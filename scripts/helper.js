require('dotenv').config()

const Eos = require('eosjs')
const R = require('ramda')
const ecc = require('eosjs-ecc')

const networks = {
  mainnet: 'aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906',
  jungle: 'e70aaab8997e1dfce58fbfac80cbbb8fecec7b99cf982a9444273cbc64c41473',
  kylin: '5fff1dae8dc8e2fc4d5b23b2c7665c97f9e9d8edf2b6485a86ba311c25639191',
  local: 'cf057bbfb72640471fd910bcb67639c22df9f92470936cddc1ade0e2f2e7dc4f',
  telosTestnet: '1eaa0824707c8c16bd25145493bf062aecddfeb56c736f6ba6397f3195f33c9f',
  telosMainnet: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11'
}

const endpoints = {
  local: 'http://0.0.0.0:8888',
  kylin: 'http://kylin.fn.eosbixin.com',
  telosTestnet: 'https://testnet.eos.miami',
  telosMainnet: 'https://node.hypha.earth'
}

const ownerAccounts = {
  local: 'owner',
  kylin: 'seedsowner11',
  telosTestnet: 'seeds',
  telosMainnet: 'seed.seeds'
}

const {
  EOSIO_NETWORK,
  EOSIO_API_ENDPOINT,
  EOSIO_CHAIN_ID
} = process.env

const chainId = EOSIO_CHAIN_ID || networks[EOSIO_NETWORK] || networks.local
const httpEndpoint = EOSIO_API_ENDPOINT || endpoints[EOSIO_NETWORK] || endpoints.local
const owner = ownerAccounts[EOSIO_NETWORK] || ownerAccounts.local

const publicKeys = {
  [networks.local]: ['EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV', 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'],
  [networks.telosMainnet]: ['EOS6H8Xd2iKMa3KEF4JAQLbHAxkQcyrYgWXjrRJMsY5yEr2Ws7DCj', 'EOS6H8Xd2iKMa3KEF4JAQLbHAxkQcyrYgWXjrRJMsY5yEr2Ws7DCj'],
  [networks.telosTestnet]: ['EOS8MHrY9xo9HZP4LvZcWEpzMVv1cqSLxiN2QMVNy8naSi1xWZH29', 'EOS8C9tXuPMkmB6EA7vDgGtzA99k1BN6UxjkGisC1QKpQ6YV7MFqm']
}
const [ ownerPublicKey, activePublicKey ] = publicKeys[chainId]

const apiKeys = {
  [networks.local]: 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV',
  [networks.telosMainnet]: 'EOS7YXUpe1EyMAqmuFWUheuMaJoVuY3qTD33WN4TrXbEt8xSKrdH9',
  [networks.telosTestnet]: 'EOS7YXUpe1EyMAqmuFWUheuMaJoVuY3qTD33WN4TrXbEt8xSKrdH9'
}
const apiPublicKey = apiKeys[chainId]

const applicationKeys = {
  [networks.local]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq',
  [networks.telosMainnet]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq',
  [networks.telosTestnet]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq'
}
const applicationPublicKey = applicationKeys[chainId]

const freePublicKey = 'EOS8UAPG5qSWetotJjZizQKbXm8dkRF2BGFyZdub8GbeRbeXeDrt9'

const account = (accountName, quantity = '0.0000 SEEDS') => ({
  type: 'account',
  account: accountName,
  creator: owner,
  publicKey: activePublicKey,
  stakes: {
    cpu: '1.0000 TLOS',
    net: '1.0000 TLOS',
    ram: 10000
  },
  quantity
})

const contract = (accountName, contractName, quantity = '0.0000 SEEDS') => ({
  ...account(accountName, quantity),
  type: 'contract',
  name: contractName,
  stakes: {
    cpu: '1.0000 TLOS',
    net: '1.0000 TLOS',
    ram: 700000
  }
})

const token = (accountName, issuer, supply) => ({
  ...contract(accountName, 'token'),
  type: 'token',
  issuer,
  supply
})

const accountsMetadata = (network) => {
  if (network == networks.local) {
    return {
      owner: account(owner),
      firstuser: account('seedsuseraaa', '10000000.0000 SEEDS'),
      seconduser: account('seedsuserbbb', '10000000.0000 SEEDS'),
      thirduser: account('seedsuserccc', '5000000.0000 SEEDS'),
      // on main net first bank has 525000000 seeds but we use 25M above for our test accounts
      firstbank: account('gift.seeds',  '500000000.0000 SEEDS'),
      secondbank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      fourthbank: account('allies.seeds','180000000.0000 SEEDS'),
      fifthbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      invites: contract('invite.seeds', 'invites'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('schdlr.seeds', 'scheduler')

    }
  } else if (network == networks.telosMainnet) {
    return {
      owner: account(owner),
      firstbank: account('gift.seeds',  '525000000.0000 SEEDS'),
      secondbank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      fourthbank: account('allies.seeds','180000000.0000 SEEDS'),
      fifthbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      invites: contract('invite.seeds', 'invites'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('schdlr.seeds', 'scheduler')

    }
  } else if (network == networks.telosTestnet) {
    return {
      firstuser: account('seedsuseraaa', '10000000.0000 SEEDS'),
      seconduser: account('seedsuserbbb', '10000000.0000 SEEDS'),
      thirduser: account('seedsuserccc', '5000000.0000 SEEDS'),

      owner: account(owner),
      // on main net first bank has 525000000 seeds but we use 25M above for our test accounts
      firstbank: account('gift.seeds',  '500000000.0000 SEEDS'),
      secondbank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      fourthbank: account('allies.seeds','180000000.0000 SEEDS'),
      fifthbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      invites: contract('invite.seeds', 'invites'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('schdlr.seeds', 'scheduler')
    }
  } else if (network == networks.kylin) {
    throw new Error('Kylin deployment currently disabled')
  } else {
    throw new Error(`${network} deployment not supported`)
  }
}

const accounts = accountsMetadata(chainId)
const names = R.mapObjIndexed((item) => item.account, accounts)

const permissions = [{
  target: `${accounts.secondbank.account}@active`,
  actor: `${accounts.proposals.account}@active`
}, {
  target: `${accounts.exchange.account}@active`,
  actor: `${accounts.exchange.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.accounts.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@owner`,
  actor: `${accounts.accounts.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.accounts.account}@eosio.code`
}, {
  target: `${accounts.harvest.account}@active`,
  actor: `${accounts.harvest.account}@eosio.code`
}, {
  target: `${accounts.proposals.account}@active`,
  actor: `${accounts.proposals.account}@eosio.code`
}, {
  target: `${accounts.bank.account}@active`,
  actor: `${accounts.harvest.account}@active`
}, {
  target: `${accounts.bank.account}@active`,
  actor: `${accounts.proposals.account}@active`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.invites.account}@active`
}, {
  target: `${accounts.invites.account}@owner`,
  actor: `${accounts.invites.account}@eosio.code`
}, {
  target: `${accounts.invites.account}@active`,
  actor: `${accounts.invites.account}@eosio.code`
}, {
  target: `${accounts.referendums.account}@active`,
  actor: `${accounts.referendums.account}@eosio.code`
}, {
  target: `${accounts.settings.account}@active`,
  actor: `${accounts.referendums.account}@active`
}, {
  target: `${accounts.history.account}@active`,
  actor: `${accounts.harvest.account}@active`
}, {
  target: `${accounts.token.account}@active`,
  actor: `${accounts.token.account}@eosio.code`
}, {
  target: `${accounts.history.account}@active`,
  actor: `${accounts.accounts.account}@active`
}, {
  target: `${accounts.accounts.account}@api`,
  key: apiPublicKey,
  parent: 'active'
}, {
  target: `${accounts.invites.account}@api`,
  key: apiPublicKey,
  parent: 'active'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'addrep'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'subrep'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'addref'
}, {
  target: `${accounts.invites.account}@api`,
  action: 'accept'
}, {
  target: `${accounts.onboarding.account}@active`,
  actor: `${accounts.onboarding.account}@eosio.code`
}, {
  target: `${accounts.onboarding.account}@owner`,
  actor: `${accounts.onboarding.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.onboarding.account}@active`,
}, {
  target: `${accounts.onboarding.account}@application`,
  key: applicationPublicKey,
  parent: 'active'
}, {
  target: `${accounts.onboarding.account}@application`,
  action: 'accept'
}, {
  target: `${accounts.history.account}@active`,
  actor: `${accounts.token.account}@active`
}, {
  target: `${accounts.acctcreator.account}@active`,
  actor: `${accounts.acctcreator.account}@eosio.code`
}, {
  target: `${accounts.acctcreator.account}@free`,
  key: freePublicKey,
  parent: 'active'
}, {
  target: `${accounts.acctcreator.account}@free`,
  action: 'create'
}, {
  target: `${accounts.scheduler.account}@active`,
  actor: `${accounts.scheduler.account}@eosio.code`
}]

const keyProviders = {
  [networks.local]: [process.env.LOCAL_PRIVATE_KEY, process.env.LOCAL_PRIVATE_KEY, process.env.APPLICATION_KEY],
  [networks.telosMainnet]: [process.env.TELOS_MAINNET_OWNER_KEY, process.env.TELOS_MAINNET_ACTIVE_KEY, process.env.APPLICATION_KEY],
  [networks.telosTestnet]: [process.env.TELOS_TESTNET_OWNER_KEY, process.env.TELOS_TESTNET_ACTIVE_KEY, process.env.APPLICATION_KEY]
}

const keyProvider = keyProviders[chainId]

const config = {
  keyProvider,
  httpEndpoint,
  chainId
}

const eos = Eos(config)

const getEOSWithEndpoint = (ep) => {
  const config = {
    keyProvider,
    httpEndpoint: ep,
    chainId
  }
  return Eos(config)
}

const encodeName = Eos.modules.format.encodeName
const decodeName = Eos.modules.format.decodeName
const getTableRows = eos.getTableRows

const getBalance = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  return Number.parseInt(balance[0])
}

const getBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  return parseFloat(balance[0])
}

const initContracts = (accounts) =>
  Promise.all(
    Object.values(accounts).map(
      account => eos.contract(account)
    )
  ).then(
    contracts => Object.assign({}, ...Object.keys(accounts).map(
      (account, index) => ({
        [account]: contracts[index]
      })
    ))
  )
  
const sha256 = Eos.modules.ecc.sha256

const isLocal = () => { return chainId == networks.local }

const ramdom64ByteHexString = async () => {
  let privateKey = await ecc.randomKey()
  const encoded = Buffer.from(privateKey).toString('hex').substring(0, 64); 
  return encoded
}

const createKeypair = async () => {
  let private = await ecc.randomKey()
  let public = await ecc.privateToPublic(private)
  return{ private, public }
}

module.exports = {
  eos, getEOSWithEndpoint, encodeName, decodeName, getBalance, getBalanceFloat, getTableRows, initContracts,
  accounts, names, ownerPublicKey, activePublicKey, apiPublicKey, permissions, sha256, isLocal, ramdom64ByteHexString, createKeypair
}

