const fetch = require('node-fetch')
const { TextEncoder, TextDecoder } = require('util')

const eosjs = require('eosjs')
const { Api, JsonRpc, Serialize } = eosjs

const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const signatureProvider = new JsSignatureProvider([keyProvider])

const { source } = require('./deploy')
const { accounts } = require('./helper')
const { keyProvider, httpEndpoint } = require('./helper')

const rpc = new JsonRpc(httpEndpoint, { fetch })

const api = new Api({ rpc, signatureProvider, textDecoder: TextDecoder(), textEncoder: TextEncoder() })

const serializeAbi = (abi) => {
    const buffer = new Serialize.SerialBuffer({
        textEncoder: api.textEncoder,
        textDecoder: api.textDecoder
    })

    const abiDefinitions = api.abiTypes.get('abi_def')

    const abiFields = abiDefinitions.fields.reduce(
        (acc, { name }) => Object.assign(acc, {
            [name]: acc[name] || []
        }),
        abi
    )

    abiDefinitions.serialize(buffer, abiFields)

    const serializedAbi = Buffer.from(buffer.asUint8Array()).toString('hex')

    return serializedAbi
}

const proposeDeploy = async (name, commit) => {
    const { code, abi } = await source(name)

    const account = accounts[name].account

    const serializedActions = await api.serializeActions([{
        account: 'eosio',
        name: 'setcode',
        data: {
            account: account,
            code: code.toString('hex'),
            vmtype: 0,
            vmversion: 0,
        }
    }, {
        account: 'eosio',
        name: 'setabi',
        data: {
            account: acccount,
            abi: serializeAbi(abi)
        }
    }])

    await api.transact({
        actions: [
            {
                account: 'msig.seeds',
                name: 'propose',
                authorization: [{
                    actor: creator,
                    permission: 'active',
                }],
                data: {
                    proposer: creator,
                    proposal_name: proposalName,
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

module.exports = { proposeDeploy }