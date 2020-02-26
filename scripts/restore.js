const { createClient, createReducer } = require('eosio-history-demux')

const { eos, names } = require('../scripts/helper')

const httpEndpoint = 'https://node.hypha.earth'

const client = createClient(httpEndpoint)

const accounts = 'seedsaccntsx'

const main = async () => {
    const contract = await eos.contract(accounts)

    const adduser = async (transaction) => {

        throw new Error("This no longer works, adduser params now have a type param")

        try {
            const { account, nickname } = transaction['action_trace']['act']['data']
    
            console.log(transaction.account_action_seq+`: trying to add user ${account} at ${accounts}`)
            await contract.adduser(account, nickname, { authorization: `${accounts}@active` })
            console.log(`🎉🎉🎉 ${account} added 🎉🎉🎉`)
        } catch (err) {
            console.error(err)
        }
    }
    
    const update = async (transaction) => {
        throw new Error("This no longer works, adduser params now have a type param")

        try {
            const { user, type, nickname, image, story, roles, skills, interests } = transaction['action_trace']['act']['data']
        
            console.log(transaction.account_action_seq+`: trying to update user ${user}`)
            await contract.update(user, type, nickname, image, story, roles, skills, interests, { authorization: `${accounts}@active` })
            console.log(`🎉🎉🎉 ${user} updated 🎉🎉🎉`)
        } catch (err) {
            console.error(err)
        }
    }
    
    const genesis = async (transaction) => {
        try {
            const { user } = transaction['action_trace']['act']['data']
        
            console.log(transaction.account_action_seq + `: trying to genesis user ${user}`)
            await contract.genesis(user, { authorization: `${accounts}@active` })
            console.log(`🎉🎉🎉 ${user} was genesised 🎉🎉🎉`)
        } catch (err) {
            console.error(err)
        }
    }
    
    const addref = async (transaction) => {
        try {
            const { referrer, invited } = transaction['action_trace']['act']['data']
        
            console.log(transaction.account_action_seq + `: trying to refer ${invited} from ${referrer}`)
            await contract.addref(referrer, invited, { authorization: `${accounts}@active` })
            console.log(`🎉🎉🎉 ${invited} from ${referrer} 🎉🎉🎉`)
        } catch (err) {
            console.error(err)
        }
    }
    
    const reducer = createReducer({
        client,
        contractName: 'accts.seeds',
        //contractName: 'seedsaccntsx',
// MAX 435!!
        fromAction: 0, // number of action in history (optional)
        batchSize: 64, // number of actions per request (optional)
        handlers: {
          // actions are processed sequentially, therefore handlers can depend on data indexed by previous handler
          //'adduser': adduser,
          //'update': update,
          'genesis': genesis,
          'addref': addref,
        }
    })
}

main()