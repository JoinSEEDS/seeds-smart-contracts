const fetch = require('node-fetch')
const { TextEncoder, TextDecoder } = require('util')

const eosjs = require('eosjs')
const { Api, JsonRpc, Serialize } = eosjs

const { source } = require('./deploy')
const { accounts } = require('./helper')
const { keyProvider, httpEndpoint } = require('./helper')

const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const signatureProvider = new JsSignatureProvider(keyProvider)

const rpc = new JsonRpc(httpEndpoint, { fetch })

const api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })

const abiToHex = (abi) => {
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

    console.dir(abi)

      abiDefinitions.serialize(buffer, abi)
      const serializedAbiHexString = Buffer.from(buffer.asUint8Array()).toString('hex')
  
    return serializedAbiHexString
}

const proposeDeploy = async (name, commit) => {
    console.log('starting deployment')

    const { code, abi } = await source(name)

    console.log("compiled code with length " + code.length)

    console.log("constructed abi with length " + abi.length)

    const contractAccount = accounts[name].account

    console.log('serialize actions')

    console.log("================================")

    const setCodeData = {
        account: contractAccount,
        code: code.toString('hex'),
        vmtype: 0,
        vmversion: 0,
    }

    const setCodeAuth = `${accounts.owner.account}@active`

    console.log(" set code for account " + setCodeData.account + " from " + setCodeAuth)

    const setAbiData = {
        account: contractAccount,
        abi: abiToHex(abi)
    }

    const setAbiAuth = `${accounts.owner.account}@active`

    console.log("set abi for account " + setAbiData.account + " from " + setAbiAuth)

    console.dir(setAbiData)

    console.log("================================")

    const serializedActions = await api.serializeActions([{
        account: 'eosio',
        name: 'setcode',
        data: setCodeData,
        authorization: setCodeAuth
    }, {
        account: 'eosio',
        name: 'setabi',
        data: setAbiData,
        authorization: setAbiAuth
    }])

    const proposeAuth = `${accounts.owner.account}@active`

    console.log('send propose transaction from ' + proposeAuth)

    await api.transact({
        actions: [
            {
                account: 'msig.seeds',
                name: 'propose',
                authorization: proposeAuth,
                data: {
                    proposer: `${accounts.owner.account}`,
                    proposal_name: "upgrade",
                    requested: [{
                        actor: accounts.owner.account,
                        permission: "active"
                    }],
                    trx: {
                        expiration: '2019-09-16T16:39:15',
                        ref_block_num: 0,
                        ref_block_prefix: 0,
                        max_net_usage_words: 0,
                        max_cpu_usage_ms: 0,
                        delay_sec: 0,
                        context_free_actions: [],
                        transaction_extensions: [],                        
                        actions: serializedActions
                    }
                }
            }
        ]
    }, {
        blocksBehind: 3,
        expireSeconds: 30,
    })

    const esr = await generateESR({
        actions: [
            {
                account: 'msig.seeds',
                name: 'approve',
                authorization: [],
                data: {
                    proposal_id: 1
                }
            }
        ]
    })

    console.log(esr)
}

module.exports = proposeDeploy