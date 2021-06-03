const assert = require('assert')
const zlib = require('zlib')

const { SigningRequest } = require('eosio-signing-request')
const fetch = require('node-fetch')
const { JsonRpc, Api, Serialize } = require('eosjs')
const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')

const { source } = require('./deploy')
const { keyProvider,  httpEndpoint, accounts } = require('./helper')

const signatureProvider = new JsSignatureProvider(keyProvider)
const rpc = new JsonRpc(httpEndpoint, { fetch })
const api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })

const abiToHex = (abi) => {
    const buffer = new Serialize.SerialBuffer({
        textEncoder: api.textEncoder,
        textDecoder: api.textDecoder,
    })

    const defs = api.abiTypes.get('abi_def')

    const fields = defs.fields.reduce(
        (result, { name }) => Object.assign(result, { [name]: result[name] || [] }),
        JSON.parse(abi)
    )

    defs.serialize(buffer, fields)

    const abiHex = Buffer.from(buffer.asUint8Array()).toString('hex')

    return abiHex
}

const buildTransaction = ({ actions, headBlock }) => ({
    expiration: Serialize.timePointSecToDate(Serialize.dateToTimePointSec(headBlock.timestamp) + 3600 * 24),
    ref_block_num: headBlock.block_num & 0xffff,
    ref_block_prefix: headBlock.ref_block_prefix,
    max_net_usage_words: 0,
    delay_sec: 0,
    context_free_actions: [],
    actions: actions,
    transaction_extensions: [],
    signatures: [],
    context_free_data: []
})

const getTrxOpts = () => ({
    blocksBehind: 3,
    expireSeconds: 30
})

const getEsrOpts = () => ({
    textEncoder: api.textEncoder,
    textDecoder: api.textDecoder,
    zlib: {
        deflateRaw: (data) => new Uint8Array(zlib.deflateRawSync(Buffer.from(data))),
        inflateRaw: (data) => new Uint8Array(zlib.inflateRawSync(Buffer.from(data))),
    },
    abiProvider: {
        getAbi: async (account) => (await api.getAbi(account))
    }
})

const getContractGuardians = () => [{
    actor: accounts.owner,
    permission: 'active'
}]

const guardianPlaceholder = "............1"

async function proposeDeploy({ contractName, proposalName }) {
    console.log('propose deployment of the contract ' + contractName)
    console.log('proposal name is ' + proposalName)

    const chainInfo = await rpc.get_info()
    const headBlock = await rpc.get_block(chainInfo.last_irreversible_block_num)
    const chainId = chainInfo.chain_id

    const contractAccount = accounts[contractName].account
    console.log('contract account is equal to ' + contractAccount)
    assert(contractAccount.length > 0, 'contract does not exist')

    const { code, abi } = await source(contractName)
    console.log('compiled code has length of ' + code.length)
    console.log('compiled abi has length of ' + abi.length)
    assert(code.length > 0 && abi.length > 0, 'compiled code invalid');

    const abiHex = await abiToHex(abi)
    console.log('transformed abi to hex representation with length ' + abiHex.length)
    assert(abiHex.length > 0, 'transform abi invalid')

    const deployActions = [{
        account: 'eosio',
        name: 'setcode',
        data: {
            account: contractAccount,
            code: code.toString('hex'),
            vmtype: 0,
            vmversion: 0
        },
        authorization: {
            account: contractAccount,
            permission: 'active'
        }
    }, {
        account: 'eosio',
        name: 'setabi',
        data: {
            account: contractAccount,
            abi: abiHex
        },
        authorization: {
            account: contractAccount,
            permission: 'active'
        }
    }]
    console.log('deploy actions payload for ' + contractAccount)
    assert((deployActions[0].data.account === contractAccount) && (deployActions[0].authorization.account === contractAccount) && (deployActions[1].data.account === contractAccount) && (deployActions[1].authorization.account === contractAccount), "deploy actions invalid account");

    const serializedDeployActions = await api.serializeActions(deployActions)
    console.log('deploy actions serialized to string with length ' + serializedDeployActions.length)
    assert(serializedDeployActions.length > 0, "invalid deploy actions serialization")

    const deployTransaction = await buildTransaction({
        actions: serializedDeployActions,
        headBlock: headBlock
    })
    console.log('deploy transaction will be expired at ' + deployTransaction.expiration.toString())
    assert(new Date(deployTransaction.expiration) > new Date(), "invalid deploy transaction expiration")

    const proposeActions = [{
        account: 'msig.seeds',
        name: 'propose',
        authorization: [{
            actor: contractName,
            permission: 'active'
        }],
        data: {
            proposer: contractName,
            proposal_name: proposalName,
            requested: getContractGuardians(),
            trx: deployTransaction
        }
    }]
    console.log('propose actions payload for ' + contractName + " by " + proposeActions[0].data.proposer)
    assert(proposeActions[0].data.proposer == contractName, "deployment should be proposed by the contract")

    const approveActions = [{
        account: 'msig.seeds',
        name: 'approve',
        data: {
            proposer: contractName,
            proposal_name: proposalName,
            level: {
                actor: guardianPlaceholder,
                permission: 'active'
            }
        },
        authorization: [{
            actor: guardianPlaceholder,
            permission: 'active'
        }]
    }]
    console.log('approve actions payload for ' + contractName)
    assert(approveActions[0].data.proposer == contractName, "approve invalid proposal")

    // await api.transact({
    //     actions: proposeActions
    // }, getTrxOptions())

    const approveTransaction = await buildTransaction({
        actions: approveActions,
        headBlock: headBlock
    })
    console.log('approve transaction will be expired at ' + approveTransaction.expiration.toString())
    assert(new Date(approveTransaction.expiration) > new Date(), "approve transaction expired")

    const request = await SigningRequest.create({
        transaction: approveTransaction,
        chainId: chainId
    }, getEsrOpts())
    console.log('signing request can be shared with guardians to approve deployment of the contract ' + contractName)

    const uri = request.encode()

    console.log(`https://eosio.to/${uri.replace('esr://', '')}`)
}

module.exports = proposeDeploy