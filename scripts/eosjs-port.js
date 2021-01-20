const eosjs = require('eosjs')
const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const { TextEncoder, TextDecoder } = require('util')
const ecc = require('eosjs/dist/eosjs-ecc-migration')
const fetch = require('node-fetch')
const { Exception } = require('handlebars')

const { Api, JsonRpc, Serialize } = eosjs

let rpc
let api

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

class Eos {

  constructor (config) {
    const {
      keyProvider,
      httpEndpoint,
      chainId
    } = config

    const signatureProvider = new JsSignatureProvider(Array.isArray(keyProvider) ? keyProvider : [keyProvider])

    rpc = new JsonRpc(httpEndpoint, { fetch });
    api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })

    this.api = api

  }

  static getEcc () {
    return ecc.ecc
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

        const { authorization } = arguments[arguments.length - 1]
        let [actor, permission] = authorization.split('@')

        const data = {}

        if (action.fields.length > 0 && arguments.length == 2 && typeof arguments[0] === 'object') {
          for (let i = 0; i < action.fields.length; i++) {
            const { name } = action.fields[i]
            data[name] = arguments[0][name]
          }
        } else {
          if ((action.fields.length + 1) != arguments.length) {
            throw new Exception(`Not enough arguments to call ${action.name} action in ${accountName} contract`)
          }
          for (let i = 0; i < action.fields.length; i++) {
            const { name } = action.fields[i]
            data[name] = arguments[i]
          }
        }

        const res = await api.transact({
          actions: [{
            account: accountName,
            name: action.name,
            authorization: [{
              actor,
              permission,
            }],
            data
          }]
        },
        {
          blocksBehind: 3,
          expireSeconds: 30,
        })

        await sleep(100)
        return res

      }
    }

    return { ...contractObj, ...functions }

  }

  async getTableRows ({
    code,
    scope,
    table,
    json = true,
    limit = 10,
    reverse = false,
    show_payer = false
  }) {
    return rpc.get_table_rows({
      json,
      code,
      scope,
      table,
      limit,
      reverse,
      show_payer
    })
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

    const res = await api.transact({
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
    })

    await sleep(100)
    return res

  }

  async setcode ({ account, code, vmtype, vmversion }, { authorization }) {

    const wasmHexString = code.toString('hex')
    let [actor, permission] = authorization.split('@')

    const res = await api.transact({
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
    })

    await sleep(100)
    return res

  }

  async transaction (trx, trxConfig={}) {
    trxConfig = { blocksBehind:3, expireSeconds:30, ...trxConfig }
    const result = await api.transact(trx, trxConfig)
    await sleep(100)
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

    const res = await api.transact({
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
    })

    await sleep(100)
    return res

  }

  async linkauth ({ account, code, type, requirement }, { authorization }) {

    let [actor, permission] = authorization.split('@')

    const res = await api.transact({
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
    })

    await sleep(100)
    return res

  }

}

module.exports = Eos