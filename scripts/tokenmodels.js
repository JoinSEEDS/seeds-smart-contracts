#!/usr/bin/env node

// install default token models into tokensmaster contract

const { eos, names, isLocal, isTestNet } = require('./helper')

const { firstuser, seconduser, tokensmaster } = names

let manager
if (isLocal() || isTestNet ) {
  manager = firstuser
} else {
  // TODO: select Mainnet manager account for tokensmaster
  manager = 'illumination'
}

const tokens = {
  seedsToken: {
    chainName: "Telos",
    contract: "token.seeds",
    symbolcode: "SEEDS",
    json: JSON.stringify ({
      name: "Seeds",
      logo: "assets/images/wallet/currency_info_cards/seeds/logo.jpg",
      precision: "4",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/seeds/background.jpg"
    })
  },
  husdToken: {
    chainName: "Telos",
    contract: "husd.hypha",
    symbolcode: "HUSD",
    json: JSON.stringify ({
      name: "HUSD",
      logo: "assets/images/wallet/currency_info_cards/husd/logo.jpg",
      precision: "2",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/husd/background.jpg"
    })
  },
  hyphaToken: {
    chainName: "Telos",
    contract: "token.hypha",
    symbolcode: "HYPHA",
    json: JSON.stringify ({
      name: "Hypha",
      logo: "assets/images/wallet/currency_info_cards/hypha/logo.jpg",
      precision: "2",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/hypha/background.jpg"
    })
  },
  localScaleToken: {
    chainName: "Telos",
    contract: "token.local",
    symbolcode: "LSCL",
    json: JSON.stringify ({
      name: "LocalScale",
      logo: "assets/images/wallet/currency_info_cards/lscl/logo.png",
      precision: "4",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/lscl/background.jpg"
    })
  },
  starsToken: {
    chainName: "Telos",
    contract: "star.seeds",
    symbolcode: "STARS",
    json: JSON.stringify ({
      name: "Stars",
      logo: "assets/images/wallet/currency_info_cards/stars/logo.jpg",
      precision: "4",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/stars/background.jpg"
    })
  },
  telosToken: {
    chainName: "Telos",
    contract: "eosio.token",
    symbolcode: "TLOS",
    json: JSON.stringify ({
      name: "Telos",
      logo: "assets/images/wallet/currency_info_cards/tlos/logo.png",
      precision: "4",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/tlos/background.png"
    })
  },
  seedsTestToken: {
    chainName: "Telos",
    contract: "token.seeds",
    symbolcode: "TESTS",
    json: JSON.stringify ({
      name: "Seeds Tests",
      logo: "assets/images/wallet/currency_info_cards/seeds/logo.jpg",
      precision: "4",
      baltitle: "Wallet Balance",
      baltitle_es: "Saldo de la Billetera",
      backgdimage: "assets/images/wallet/currency_info_cards/seeds/background.jpg"
    })
  },
}


const main = async () => {

  const contract = await eos.contract(tokensmaster)

  try {
    console.log('reset')
    await contract.reset({ authorization: `${tokensmaster}@active` })
    let verification = !isLocal()
    console.log(`init - manager is ${manager}, verification is ${verification}`)
    await contract.init('Telos', manager, verification, { authorization: `${tokensmaster}@active` })
  } catch(err) {
    console.log(err)
  }

  let id = 0
  for (const key in tokens) {
    console.log(`processing ${key}`)
    data = tokens[key]
    console.log(data)
    try {
      await contract.submittoken(seconduser, data.chainName, data.contract,
         data.symbolcode, data.json, { authorization: `${seconduser}@active` })
      await contract.accepttoken(id, data.symbolcode, 'lightwallet', true, { authorization: `${tokensmaster}@active` })
      ++id
    } catch (err) {
      console.log(err)
    }
  }

  console.log('done')
}

main()

