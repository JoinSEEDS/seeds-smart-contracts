const eosjs = require('eosjs')
const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const { TextEncoder, TextDecoder } = require('util')
const fetch = require('node-fetch')
const { Exception } = require('handlebars')
const { option } = require('commander')
const { transactionHeader } = require('eosjs/dist/eosjs-serialize')
const ecc = require('eosjs-ecc')

const { Api, JsonRpc, Serialize } = eosjs

let rpc
let api
let isUnitTest

function sleep (ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// TODO: We need to only use nonce in testnet, not in mainnet. Mainnet its causing 
// errors, and we don't need it. It's only for unit tests. 
async function getNonce () {
  try {
    if (isUnitTest) {
      await rpc.getRawAbi('policy.seeds')
      const random = Math.random().toString(36).substring(4);
      return [{
        // this is a nonce action - prevents duplicate transaction errors - we borrow policy.seeds for this
        account:"policy.seeds",
        name:"create",
        authorization: [
          {
            actor: 'policy.seeds',
            permission: 'active'
          }
        ],
        data:{
          account:"policy.seeds",
          backend_user_id: random,
          device_id: random,
          signature: "",
          policy: "CREATED BY UNIT TEST"
        }
      }]
    }
    return []
  } catch (err) {
    return []
  }
}

class Eos {

  constructor (config, isLocal=null) {
    const {
      keyProvider,
      httpEndpoint,
      chainId
    } = config

    const signatureProvider = new JsSignatureProvider(Array.isArray(keyProvider) ? keyProvider : [keyProvider])

    rpc = new JsonRpc(httpEndpoint, { fetch });
    api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })

    this.api = api

    isUnitTest = isLocal ? isLocal() : false

  }

  static getEcc () {
    return ecc
  }

  async getInfo () {
    return rpc.get_info()
  }

  async contract (accountName) {

    if (typeof accountName === 'object') {
      accountName = accountName.account
    }

    const contractObj = await api.getContract(accountName)
    const actions = contractObj.actions
    const functions = {}

    for (let [key, action] of actions) {
      functions[action.name] = async function () {

        let auth
        const { authorization } = arguments[arguments.length - 1]
        if (Array.isArray(authorization)) {
          auth = authorization
        } else {
          let [actor, permission] = authorization.split('@')
          auth = [{
            actor,
            permission,
          }]
        }

        const data = {}

        if ((action.fields.length + 1) != arguments.length) {
          throw new Exception(`Not enough arguments to call ${action.name} action in ${accountName} contract`)
        }
        for (let i = 0; i < action.fields.length; i++) {
          const { name } = action.fields[i]
          data[name] = arguments[i]
        }

        const nonce = await getNonce() 
        const actions = [
          {
          account: accountName,
          name: action.name,
          authorization: auth,
            data
          },
          ...nonce
        ]

        const trxConfig = {
          blocksBehind: 3,
          expireSeconds: 30,
        }

        let res
        try {
          res = await api.transact({
            actions
          }, trxConfig)
        }
        catch (err) {
          const errStr = ''+err
          console.log('TRANSACTION ERROR:', errStr)
          if (errStr.toLowerCase().includes('deadline exceeded')) {
            await sleep(3000)
            console.log('retrying...')
            res = await api.transact({
              actions
            }, trxConfig)
          } else {
            console.log("Error on actions: "+JSON.stringify(actions))
            throw err
          }
        }

        return res

      }
    }

    return { ...contractObj, ...functions }

  }

  async getTableRows (namedParameters) {
    return rpc.get_table_rows(namedParameters)
  }

  async getCurrencyBalance (contract, account, token) {
    return rpc.get_currency_balance(contract, account, token)
  }

  async setabi ({ account, abi }, { authorization }) {

    const buffer = new Serialize.SerialBuffer({
      textEncoder: api.textEncoder,
      textDecoder: api.textDecoder,
    })

    const abiDefinitions = api.abiTypes.get('abi_def')
    abi = abiDefinitions.fields.reduce(
      (acc, { name: fieldName }) =>
          Object.assign(acc, { [fieldName]: acc[fieldName] || [] }),
          abi
      )
    abiDefinitions.serialize(buffer, abi)
    const serializedAbiHexString = Buffer.from(buffer.asUint8Array()).toString('hex')

    let [actor, permission] = authorization.split('@')

    const res = await this.transaction({
      actions: [{
        account: 'eosio',
        name: 'setabi',
        authorization: [{
          actor,
          permission,
        }],
        data: {
          account,
          abi: serializedAbiHexString
        }
      }]
    },
    {
      blocksBehind: 3,
      expireSeconds: 30,
    }, 3)

    await sleep(100)
    return res

  }

  async setcode ({ account, code, vmtype, vmversion }, { authorization }) {

    const wasmHexString = code.toString('hex')
    let [actor, permission] = authorization.split('@')

    const res = await this.transaction({
      actions: [{
        account: 'eosio',
        name: 'setcode',
        authorization: [{
          actor,
          permission,
        }],
        data: {
          account,
          code: wasmHexString,
          vmtype,
          vmversion
        }
      }]
    },
    {
      blocksBehind: 3,
      expireSeconds: 30,
    }, 3)

    await sleep(100)
    return res

  }

  async transaction (trx, trxConfig={}, numTries=0) {
    trxConfig = { blocksBehind:3, expireSeconds:30, ...trxConfig }
    let result
    try {
      result = await api.transact(trx, trxConfig)
    } catch (err) {
      const errStr = '' + err
      if (errStr.toLowerCase().includes('exceeded by') && numTries > 0) {
        console.error(errStr, ', retrying...')
        await sleep(100)
        return await this.transaction(trx, trxConfig, numTries-1)
      }
      throw err
    }
    return result
  }

  async getAccount (account) {
    return rpc.get_account(account)
  }

  async getActions (account, pos, offset) {
    return rpc.history_get_actions(account, pos, offset)
  }

  async updateauth ({ account, permission, parent, auth }, { authorization }) {

    let [actor, perm] = authorization.split('@')

    if ((parent == 'owner' && permission == 'owner') || parent == '') {
      parent = '.'
    }

    auth.accounts.sort((a, b) => {
      if (a.permission.actor <= b.permission.actor) {
        return -1
      }
      return 1
    })

    const res = await this.transaction({
      actions: [
      {
        account: 'eosio',
        name: 'updateauth',
        authorization: [{
          actor,
          permission: perm,
        }],
        data: {
          account,
          permission,
          parent,
          auth
        },
      }]
    }, {
      blocksBehind: 3,
      expireSeconds: 30,
    }, 3)

    await sleep(100)
    return res

  }

  async linkauth ({ account, code, type, requirement }, { authorization }) {

    let [actor, permission] = authorization.split('@')

    const res = await this.transaction({
      actions: [{
        account: 'eosio',
        name: 'linkauth',
        authorization: [{
          actor,
          permission,
        }],
        data: {
          account,
          code,
          type,
          requirement
        },
      }]
    }, {
      blocksBehind: 3,
      expireSeconds: 30,
    }, 3)

    await sleep(100)
    return res

  }

}

module.exports = Eos