const fs = require('fs')
const path = require('path')

const { eos, encodeName, accounts, ownerPublicKey, activePublicKey } = require('./helper')

const deploy = async (name) => {
  try {
    const { code, abi } = await source(name)

    let account = accounts[name].name

    console.log("acct ", JSON.stringify(accounts[name]));

    if (!code)
      throw new Error('code not found')

    if (!abi)
      throw new Error('abi not found')

    await eos.setcode({
      account,
      code,
      vmtype: 0,
      vmversion: 0
    }, {
      authorization: `${account}@owner`
    })

    await eos.setabi({
      account,
      abi: JSON.parse(abi)
    }, {
      authorization: `${account}@owner`
    })
    console.log(`${name} deployed to ${account}`)
  } catch (err) {
    console.error(`account ${name} already deployed`, err)
  }
}

const source = async (name) => {
  const codePath = path.join(__dirname, '../artifacts', name.concat('.wasm'))
  const abiPath = path.join(__dirname, '../artifacts', name.concat('.abi'))

  const code = new Promise(resolve => {
    fs.readFile(codePath, (_, r) => resolve(r))
  })
  const abi = new Promise(resolve => {
    fs.readFile(abiPath, (_, r) => resolve(r))
  })

  return Promise.all([code, abi]).then(([code, abi]) => ({ code, abi }))
}

module.exports = deploy