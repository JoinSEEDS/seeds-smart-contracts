const fetch = require('node-fetch')
const { TextEncoder, TextDecoder } = require('util')

const eosjs = require('eosjs')
const { Api, JsonRpc, Serialize } = eosjs

const { source } = require('./deploy')
const { accounts, eos } = require('./helper')

// const Eos = require('./eosjs-port')

// let api = Eos.api

const abiToHex = (abi) => {
    const api = eos.api

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
  
    return serializedAbiHexString
}

const proposeDeploy = async (name, commit) => {
    console.log('starting deployment')

    const api = eos.api

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

    const setCodeAuth = [{
                actor: contractAccount,
                permission: 'active',
    }]

    console.log(" set code for account " + setCodeData.account + " from " + setCodeAuth)

    console.log(" setting abi " +contractAccount +"\nABI:"+ abi)

    const abiAsHex = await abiToHex(JSON.parse(abi))

    console.log("hex abi: "+abiAsHex)

    const setAbiData = {
        account: contractAccount,
        abi: abiAsHex
    }

    const setAbiAuth = [{
        actor: contractAccount,
        permission: 'active',   
    }]

    const actions = [{
        account: 'eosio',
        name: 'setcode',
        data: setCodeData,
        authorization: setCodeAuth
    }, {
        account: 'eosio',
        name: 'setabi',
        data: setAbiData,
        authorization: setAbiAuth
    }]
    // console.log("Actions:")
    // console.log(actions)

    const serializedActions = await api.serializeActions(actions)

    // console.log("SER Actions:")
    // console.log(serializedActions)


    // * @param proposer - The account proposing a transaction
    // * @param proposal_name - The name of the proposal (should be unique for proposer)
    // * @param requested - Permission levels expected to approve the proposal
    // * @param trx - Proposed transaction

    console.log("====== PROPOSING ======")

    const proposerAccount = "msig.seeds"

    const proposeInput = {
        proposer: proposerAccount,
        proposal_name: 'propose',
        requested: [ // NOTE: Ignored
            {
                actor: proposerAccount,
                permission: "active"
            },
          {
            actor: 'useraaaaaaaa',
            permission: 'active'
          },
          {
            actor: 'userbbbbbbbb',
            permission: 'active'
          }
        ],
        trx: {
          expiration: '2021-09-14T16:39:15',
          ref_block_num: 0,
          ref_block_prefix: 0,
          max_net_usage_words: 0,
          max_cpu_usage_ms: 0,
          delay_sec: 0,
          context_free_actions: [],
          actions: serializedActions,
          transaction_extensions: []
        }
      };

      console.log('send propose ' + JSON.stringify(proposeInput))

      console.log("====== TRANSACT  ======")

      const auth  = [{
        actor: proposerAccount,
        permission: 'active',
      }]

    const propActions = [{
        account: 'msig.seeds',
        name: 'propose',
        authorization: auth,
        data: proposeInput
    }]

      const trxConfig = {
        blocksBehind: 3,
        expireSeconds: 30,
      }

      let res = await api.transact({
            actions: propActions
        }, trxConfig)

    //   const tx_id = await api.transact({
    //     actions: [{
    //       account: 'msig.seeds',
    //       name: 'propose',
    //       authorization: [{
    //         actor: '${accounts.owner.account}',
    //         permission: 'active',
    //       }],
    //       data: proposeInput,
    //     }]
    //   }, {
    //     blocksBehind: 3,
    //     expireSeconds: 30,
    //     broadcast: true,
    //     sign: true
    //   });

      console.log("propose success "+res)
      
    // const esr = await generateESR({
    //     actions: [
    //         {
    //             account: 'msig.seeds',
    //             name: 'approve',
    //             authorization: [],
    //             data: {
    //                 proposal_id: 1
    //             }
    //         }
    //     ]
    // })

    // console.log(esr)
}

module.exports = proposeDeploy