require('dotenv').config()

const Eos = require('./eosjs-port')
const R = require('ramda')

// Note: For some reason local chain ID is different on docker vs. local install of eosio
const dockerLocalChainID = 'cf057bbfb72640471fd910bcb67639c22df9f92470936cddc1ade0e2f2e7dc4f'
const eosioLocalChainID = '8a34ec7df1b8cd06ff4a8abbaa7cc50300823350cadc59ab296cb00d104d2b8f'

const networks = {
  mainnet: 'aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906',
  jungle: 'e70aaab8997e1dfce58fbfac80cbbb8fecec7b99cf982a9444273cbc64c41473',
  kylin: '5fff1dae8dc8e2fc4d5b23b2c7665c97f9e9d8edf2b6485a86ba311c25639191',
  // local: process.env.COMPILER === 'local' ? eosioLocalChainID : dockerLocalChainID,
  local: 'cf057bbfb72640471fd910bcb67639c22df9f92470936cddc1ade0e2f2e7dc4f',
  telosTestnet: '1eaa0824707c8c16bd25145493bf062aecddfeb56c736f6ba6397f3195f33c9f',
  telosMainnet: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11'
}

const networkDisplayName = {
  mainnet: '???',
  jungle: 'Jungle',
  kylin: 'Kylin',
  local: 'Local',
  telosTestnet: 'Telos Testnet',
  telosMainnet: 'Telos Mainnet'
}

const endpoints = {
  local: 'http://127.0.0.1:8888',
  kylin: 'http://kylin.fn.eosbixin.com',
  telosTestnet: 'https://test.hypha.earth',
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

const netName = EOSIO_NETWORK != undefined ? (networkDisplayName[EOSIO_NETWORK] || "INVALID NETWORK: "+EOSIO_NETWORK) : "Local"
console.log(""+netName)

const publicKeys = {
  [networks.local]: ['EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV', 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'],
  [networks.telosMainnet]: ['EOS6kp3dm9Ug5D3LddB8kCMqmHg2gxKpmRvTNJ6bDFPiop93sGyLR', 'EOS6kp3dm9Ug5D3LddB8kCMqmHg2gxKpmRvTNJ6bDFPiop93sGyLR'],
  [networks.telosTestnet]: ['EOS8MHrY9xo9HZP4LvZcWEpzMVv1cqSLxiN2QMVNy8naSi1xWZH29', 'EOS8C9tXuPMkmB6EA7vDgGtzA99k1BN6UxjkGisC1QKpQ6YV7MFqm']
  // NOTE: Testnet seems to use EOS8C9tXuPMkmB6EA7vDgGtzA99k1BN6UxjkGisC1QKpQ6YV7MFqm for onwer and active - verify
}
const [ ownerPublicKey, activePublicKey ] = publicKeys[chainId]

const apiKeys = {
  [networks.local]: 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV',
  [networks.telosMainnet]: 'EOS7YXUpe1EyMAqmuFWUheuMaJoVuY3qTD33WN4TrXbEt8xSKrdH9',
  [networks.telosTestnet]: 'EOS7YXUpe1EyMAqmuFWUheuMaJoVuY3qTD33WN4TrXbEt8xSKrdH9'
}
const apiPublicKey = apiKeys[chainId]


const payForCPUKeys = {
  [networks.local]: 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV',
  [networks.telosMainnet]: 'EOS8gu3qzDsieAC7ni7o9vdKKnUjQXMEXN1NQNjFFs6M2u2kEyTvz',
  [networks.telosTestnet]: 'EOS8CE5iqFh5XNfJygGZjm7FtKRSLEHFHfioXF6VLmoQSAMSrzzXE'
}

const payForCPUPublicKey = payForCPUKeys[chainId]

const applicationKeys = {
  [networks.local]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq',
  [networks.telosMainnet]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq',
  [networks.telosTestnet]: 'EOS7HXZn1yhQJAiHbUXeEnPTVHoZLgAScNNELAyvWxoqQJzcLbbjq'
}
const applicationPublicKey = applicationKeys[chainId]

const exchangeKeys = {
  [networks.local]: 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV', // normal dev key
  [networks.telosMainnet]: 'EOS75DmTxcnpvhjNekfKQzLrfwo44muPN6YPPX49vYPot4Qmo5cTo', 
  [networks.telosTestnet]: 'EOS8C9tXuPMkmB6EA7vDgGtzA99k1BN6UxjkGisC1QKpQ6YV7MFqm' 
}

const exchangePublicKey = exchangeKeys[chainId]

const freePublicKey = 'EOS8UAPG5qSWetotJjZizQKbXm8dkRF2BGFyZdub8GbeRbeXeDrt9'

const account = (accountName, quantity = '0.0000 SEEDS', pubkey = activePublicKey) => ({
  type: 'account',
  account: accountName,
  creator: owner,
  publicKey: pubkey,
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

const testnetUserPubkey = "EOS8M3bWwv7jvDGpS2avYRiYh2BGJxt5VhfjXhbyAhFXmPtrSd591"

const token = (accountName, issuer, supply) => ({
  ...contract(accountName, 'token'),
  type: 'token',
  issuer,
  supply
})

const bdc = 'bdc'

const accountsMetadata = (network) => {
  if (network == networks.local) {
    return {
      owner: account(owner),
      bdc: account(bdc),
      firstuser: account('seedsuseraaa', '10000000.0000 SEEDS'),
      seconduser: account('seedsuserbbb', '10000000.0000 SEEDS'),
      thirduser: account('seedsuserccc', '5000000.0000 SEEDS'),
      fourthuser: account('seedsuserxxx', '10000000.0000 SEEDS'),
      fifthuser: account('seedsuseryyy', '10000000.0000 SEEDS'),
      sixthuser: account('seedsuserzzz', '5000000.0000 SEEDS'),

      // on main net first bank has 525000000 seeds but we use 25M above for our test accounts
      campaignbank: account('gift.seeds',  '500000000.0000 SEEDS'),
      milestonebank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      alliancesbank: account('allies.seeds','180000000.0000 SEEDS'),
      ambassadorsandreferralsbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      globaldho: account('gdho.seeds'),
      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      testtoken: token('token.seeds', owner, '1500000000.0000 TESTS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      escrow: contract('escrow.seeds', 'escrow'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('cycle.seeds', 'scheduler'),
      organization: contract('orgs.seeds', 'organization'),
      bioregion: contract('bio.seeds', 'bioregion'),
      msig: contract('msig.seeds', 'msig'),
      guardians: contract('guard.seeds', 'guardians'),
      pouch: contract('pouch.seeds', 'pouch'),
      quests: contract('quests.seeds', 'quests')
    }
  } else if (network == networks.telosMainnet) {
    return {
      owner: account(owner),
      bdc: account(bdc),
      campaignbank: account('gift.seeds',  '525000000.0000 SEEDS'),
      milestonebank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      alliancesbank: account('allies.seeds','180000000.0000 SEEDS'),
      ambassadorsandreferralsbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      globaldho: account('gdho.seeds'),
      testtoken: token('token.seeds', owner, '1500000000.0000 TESTS'),

      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      escrow: contract('escrow.seeds', 'escrow'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('cycle.seeds', 'scheduler'),
      organization: contract('orgs.seeds', 'organization'),
      bioregion: contract('bio.seeds', 'bioregion'),
      msig: contract('msig.seeds', 'msig'),
      guardians: contract('guard.seeds', 'guardians'),
      pouch: contract('pouch.seeds', 'pouch'),
    }
  } else if (network == networks.telosTestnet) {
    return {
      firstuser: account('seedsuseraaa', '10000000.0000 SEEDS'),
      seconduser: account('seedsuserbbb', '10000000.0000 SEEDS'),
      thirduser: account('seedsuserccc', '5000000.0000 SEEDS'),
      fourthuser: account('seedsuserxxx', '10000000.0000 SEEDS', testnetUserPubkey),
      fifthuser: account('seedsuseryyy', '10000000.0000 SEEDS', testnetUserPubkey),
      sixthuser: account('seedsuserzzz', '5000000.0000 SEEDS', testnetUserPubkey),

      owner: account(owner),
      bdc: account(bdc),

      campaignbank: account('gift.seeds',  '500000000.0000 SEEDS'),
      milestonebank: account('milest.seeds', '75000000.0000 SEEDS'),
      thirdbank: account('hypha.seeds',  '300000000.0000 SEEDS'),
      alliancesbank: account('allies.seeds','180000000.0000 SEEDS'),
      ambassadorsandreferralsbank: account('refer.seeds',  '120000000.0000 SEEDS'),
      sixthbank: account('bank.seeds',   '300000000.0000 SEEDS'),
      bank: account('system.seeds'),
      globaldho: account('gdho.seeds'),

      history: contract('histry.seeds', 'history'),
      accounts: contract('accts.seeds', 'accounts'),
      harvest: contract('harvst.seeds', 'harvest'),
      settings: contract('settgs.seeds', 'settings'),
      proposals: contract('funds.seeds', 'proposals'),
      referendums: contract('rules.seeds', 'referendums'),
      token: token('token.seeds', owner, '1500000000.0000 SEEDS'),
      testtoken: token('token.seeds', owner, '1500000000.0000 TESTS'),
      policy: contract('policy.seeds', 'policy'),
      onboarding: contract('join.seeds', 'onboarding'),
      acctcreator: contract('free.seeds', 'acctcreator'),
      exchange: contract('tlosto.seeds', 'exchange'),
      escrow: contract('escrow.seeds', 'escrow'),
      forum: contract('forum.seeds', 'forum'),
      scheduler: contract('cycle.seeds', 'scheduler'),
      organization: contract('orgs.seeds', 'organization'),
      bioregion: contract('bio.seeds', 'bioregion'),
      msig: contract('msig.seeds', 'msig'),
      guardians: contract('guard.seeds', 'guardians'),
      pouch: contract('pouch.seeds', 'pouch'),
    }
  } else if (network == networks.kylin) {
    throw new Error('Kylin deployment currently disabled')
  } else {
    throw new Error(`${network} deployment not supported`)
  }
}

const accounts = accountsMetadata(chainId)
const names = R.mapObjIndexed((item) => item.account, accounts)
const allContracts = []
const allContractNames = []
const allAccounts = []
const allBankAccountNames = []
for (let [key, value] of Object.entries(names)) {
  if (accounts[key].type=="contract" || accounts[key].type=="token") {
    allContracts.push(key)
    allContractNames.push(value)
  } else {
    if (value.indexOf(".seeds") != -1) {
      allAccounts.push(key)
      allBankAccountNames.push(value)
    }
  }
}
allContracts.sort()
allContractNames.sort()
allAccounts.sort()
allBankAccountNames.sort()

var permissions = [{
  target: `${accounts.campaignbank.account}@active`,
  actor: `${accounts.proposals.account}@active`
}, {
  target: `${accounts.milestonebank.account}@active`,
  actor: `${accounts.proposals.account}@active`
}, {
  target: `${accounts.alliancesbank.account}@active`,
  actor: `${accounts.proposals.account}@active`
}, {
  target: `${accounts.ambassadorsandreferralsbank.account}@active`,
  actor: `${accounts.accounts.account}@active`
}, {
  target: `${accounts.globaldho.account}@active`,
  actor: `${accounts.harvest.account}@active`
}, {
  target: `${accounts.exchange.account}@active`,
  actor: `${accounts.exchange.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@owner`,
  actor: `${accounts.accounts.account}@eosio.code`
}, {
  target: `${accounts.accounts.account}@active`,
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
  target: `${accounts.proposals.account}@active`,
  actor: `${accounts.accounts.account}@active`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.proposals.account}@eosio.code`
}, {
  target: `${accounts.bank.account}@active`,
  actor: `${accounts.proposals.account}@active`
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
  target: `${accounts.harvest.account}@active`,
  actor: `${accounts.history.account}@eosio.code`
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
  target: `${accounts.exchange.account}@purchase`,
  key: exchangePublicKey,
  parent: 'active'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'addrep'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'subrep'
}, {
  target: `${accounts.harvest.account}@setorgtxpt`,
  actor: `${accounts.history.account}@eosio.code`,
  parent: 'active',
  type: 'createActorPermission'
}, {
  target: `${accounts.harvest.account}@setorgtxpt`,
  action: 'setorgtxpt'
}, {
  target: `${accounts.accounts.account}@api`,
  action: 'addref'
}, {
  target: `${accounts.exchange.account}@purchase`,
  action: 'newpayment'
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
  target: `${accounts.onboarding.account}@active`,
  actor: `${accounts.organization.account}@active`,
}, {
  target: `${accounts.onboarding.account}@active`,
  actor: `${accounts.bioregion.account}@active`,
}, {
  target: `${accounts.onboarding.account}@application`,
  key: applicationPublicKey,
  parent: 'active'
}, {
  target: `${accounts.onboarding.account}@application`,
  action: 'numtrx'
}, {
  target: `${accounts.onboarding.account}@application`,
  action: 'accept'
}, {
  target: `${accounts.guardians.account}@application`,
  key: applicationPublicKey,
  parent: 'active'
}, {
  target: `${accounts.guardians.account}@application`,
  action: 'claim'
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
}, {
  target: `${accounts.organization.account}@active`,
  actor: `${accounts.organization.account}@eosio.code`
}, {
  target: `${accounts.forum.account}@active`,
  actor: `${accounts.forum.account}@eosio.code`
}, {
  target: `${accounts.forum.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.scheduler.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.forum.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.forum.account}@execute`,
  action: 'onperiod'
}, {
  target: `${accounts.proposals.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.proposals.account}@execute`,
  action: 'onperiod'
}, {
  target: `${accounts.accounts.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.accounts.account}@execute`,
  action: 'rankreps'
}, {
  target: `${accounts.accounts.account}@execute`,
  action: 'rankcbss'
}, {
  target: `${accounts.accounts.account}@execute`,
  target: `${accounts.token.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.token.account}@execute`,
  action: 'resetweekly'
}, {
  target: `${accounts.token.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.forum.account}@execute`,
  action: 'newday'
}, {
  target: `${accounts.harvest.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.exchange.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.harvest.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.accounts.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.bdc.account}@owner`,
  actor: `${accounts.onboarding.account}@eosio.code`
}, {
  target: `${accounts.bioregion.account}@active`,
  actor: `${accounts.bioregion.account}@eosio.code`
}, {
  target: `${accounts.exchange.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.scheduler.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.harvest.account}@execute`, 
  action: 'ranktxs'
}, {
  target: `${accounts.harvest.account}@execute`, 
  action: 'rankplanteds'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'calccss'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'rankcss'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'calctrxpts'
}, {
  target: `${accounts.exchange.account}@execute`,
  action: 'onperiod'
}, {
  target: `${accounts.scheduler.account}@execute`,
  action: 'test1'
}, {
  target: `${accounts.scheduler.account}@execute`,
  action: 'test2'
}, {
  target: `${accounts.referendums.account}@execute`, // TODO these active keys are not needed?!
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.referendums.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.proposals.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.token.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.token.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.token.account}@execute`,
  action: 'resetweekly'
}, {
  target: `${accounts.onboarding.account}@application`,
  action: 'acceptnew'
}, {
  target: `${accounts.onboarding.account}@application`,
  action: 'acceptexist'
}, {
  target: `${accounts.harvest.account}@payforcpu`,
  key: payForCPUPublicKey,
  parent: 'active'
}, {
  target: `${accounts.harvest.account}@payforcpu`,
  action: 'payforcpu'
}, {
  target: `${accounts.escrow.account}@active`,
  actor: `${accounts.escrow.account}@eosio.code`
}, {
  target: `${accounts.organization.account}@execute`,
  key: activePublicKey,
  parent: 'active'
}, {
  target: `${accounts.organization.account}@execute`,
  action: 'cleandaus'
}, { 
  target: `${accounts.organization.account}@execute`,
  actor: `${accounts.scheduler.account}@active`
}, {
  target: `${accounts.organization.account}@active`,
  actor: `${accounts.accounts.account}@active`
}, {
  target: `${accounts.organization.account}@execute`,
  action: 'scoretrxs'
}, {
  target: `${accounts.organization.account}@execute`,
  action: 'rankregens'
}, {
  target: `${accounts.organization.account}@execute`,
  action: 'rankcbsorgs'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'rankorgtxs'
}, {
  target: `${accounts.proposals.account}@execute`,
  action: 'decayvoices'
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.forum.account}@active`
}, {
  target: `${accounts.forum.account}@execute`,
  action: 'rankforums'
}, {
  target: `${accounts.forum.account}@execute`,
  action: 'givereps'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'calcmqevs'
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'calcmintrate'
}, {
  target: `${accounts.token.account}@minthrvst`,
  actor: `${accounts.harvest.account}@eosio.code`,
  parent: 'active',
  type: 'createActorPermission'
}, {
  target: `${accounts.token.account}@minthrvst`,
  action: 'minttst'
}, { 
  target: `${accounts.harvest.account}@active`,
  actor: `${accounts.organization.account}@active`
}, {
  target: `${accounts.harvest.account}@execute`,
  action: 'rankbiocss'
}, {
  target: `${accounts.pouch.account}@active`,
  actor: `${accounts.pouch.account}@eosio.code`
}, {
  target: `${accounts.bank.account}@active`,
  actor: `${accounts.pouch.account}@active`
}, {
  target: `${accounts.quests.account}@active`,
  actor: `${accounts.quests.account}@eosio.code`
}, {
  target: `${accounts.campaignbank.account}@active`,
  actor: `${accounts.quests.account}@active`
}, {
  target: `${accounts.proposals.account}@active`,
  actor: `${accounts.quests.account}@active`
}, {
  target: `${accounts.accounts.account}@active`,
  actor: `${accounts.quests.account}@active`
}]

const isTestnet = chainId == networks.telosTestnet
const isLocalNet = chainId == networks.local

if (isTestnet || isLocalNet) {
  //console.log("Adding TESTNET permissions")
  const testnetDevelopmentKey = 'EOS7WSioF5yu8yoKEvnaryCJBSSqgdEiPLxHxGwYnvQXbYddTrUts'
  permissions.push({
      target: `${accounts.proposals.account}@testnetdev`,
      key: testnetDevelopmentKey,
      parent: 'active'
  })
  // Note: This overrides @execute permission on onperiod - this means that on testnet, the proposals contract 
  // doesn't work with the scheduler
  permissions.push({
      target: `${accounts.proposals.account}@testnetdev`,
      action: 'onperiod'
  })
}

const keyProviders = {
  [networks.local]: [process.env.LOCAL_PRIVATE_KEY, process.env.LOCAL_PRIVATE_KEY, process.env.APPLICATION_KEY],
  [networks.telosMainnet]: [process.env.TELOS_MAINNET_OWNER_KEY, process.env.TELOS_MAINNET_ACTIVE_KEY, process.env.APPLICATION_KEY, process.env.EXCHANGE_KEY,process.env.PAY_FOR_CPU_MAINNET_KEY,process.env.SCRIPT_KEY],
  [networks.telosTestnet]: [process.env.TELOS_TESTNET_OWNER_KEY, process.env.TELOS_TESTNET_ACTIVE_KEY, process.env.APPLICATION_KEY]
}

const keyProvider = keyProviders[chainId]


if (keyProvider.length == 0 || keyProvider[0] == null) {
  console.log("ERROR: Invalid Key Provider: "+JSON.stringify(keyProvider, null, 2))
}

const config = {
  keyProvider,
  httpEndpoint,
  chainId
}

const eos = new Eos(config)

setTimeout(async ()=>{
  let info = await eos.getInfo({})
  if (info.chain_id != chainId) {
    console.error("Fix this by setting local chain ID to "+info.chain_id)
    console.error('Chain ID mismatch, signing will not work - \nactual Chain ID: "+info.chain_id + "\nexpected Chain ID: "+chainId')
    throw new Error("Chain ID mismatch")
  }
})

const getEOSWithEndpoint = (ep) => {
  const config = {
    keyProvider,
    httpEndpoint: ep,
    chainId
  }
  return new Eos(config)
}

// ===========================================================================
// This methods not exist anymore, but they aren't used throughout the project
const encodeName = null // Eos.modules.format.encodeName
const decodeName = null // Eos.modules.format.decodeName
// ===========================================================================

const getTableRows = eos.getTableRows

const getTelosBalance = async (user) => {
  const balance = await eos.getCurrencyBalance(names.tlostoken, user, 'TLOS')
  return Number.parseInt(balance[0])
}

const getBalance = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  return Number.parseInt(balance[0])
}

const getBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  var float = parseInt(Math.round(parseFloat(balance[0]) * 10000)) / 10000.0;

  return float;
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
  
const sha256 = Eos.getEcc().sha256

const isLocal = () => { return chainId == networks.local }

const ramdom64ByteHexString = async () => {
  let privateKey = await Eos.getEcc().randomKey({}, {
    secureEnv: true
  })
  const encoded = Buffer.from(privateKey).toString('hex').substring(0, 64); 
  return encoded
}
const fromHexString = hexString => new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))

const createKeypair = async () => {
  let private = await Eos.getEcc().randomKey({}, {
    secureEnv: true
  })
  let public = await Eos.getEcc().privateToPublic(private)
  return{ private, public }
}

const sleep = async (ms) => {
  return new Promise(resolve => setTimeout(resolve, ms));
}

const eoslime = require('eoslime').init({ url: httpEndpoint, chainId })

const eoslimeAccounts = {}

for (let [key, value] of Object.entries(names)) {
  eoslimeAccounts[`${key}Account`] = eoslime.Account.load(key, keyProvider[1])
}

module.exports = {
  eos, getEOSWithEndpoint, encodeName, decodeName, getBalance, getBalanceFloat, getTableRows, initContracts,
  accounts, names, ownerPublicKey, activePublicKey, apiPublicKey, permissions, sha256, isLocal, ramdom64ByteHexString, createKeypair,
  testnetUserPubkey, getTelosBalance, fromHexString, allContractNames, allContracts, allBankAccountNames, sleep, eoslimeAccounts
}

