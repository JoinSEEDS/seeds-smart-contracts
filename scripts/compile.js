const { exec } = require('child_process')

const command = ({ contract, source }) => {
    const volume = '/home/sevenflash/apps/seeds-contracts'

    return `docker run --rm --name eosio.cdt_v1.6.1 --volume ${volume}:/project -w /project eostudio/eosio.cdt:v1.6.1 /bin/bash -c "eosio-cpp -abigen -I ./include -contract ${contract} -o ./artifacts/${contract}.wasm ${source}"`
}

const compile = (contract) => {
    return new Promise((resolve, reject) => {
        exec(command(contract), (error, stdout, stderr) => {
          if (error) return reject(error)

          resolve()
        })
    })
}

const main = async () => {
await compile({
    contract: 'seeds.token',
    source: './src/seeds.token.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})

await compile({
    contract: 'harvest',
    source: './src/seeds.harvest.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})

await compile({
    contract: 'accounts',
    source: './src/seeds.accounts.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})

await compile({
    contract: 'proposals',
    source: './src/seeds.proposals.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})

await compile({
    contract: 'settings',
    source: './src/seeds.settings.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})

await compile({
    contract: 'subscription',
    source: './src/seeds.subscription.cpp'
}).then(result => {
    console.log({ result })
}).catch(err => {
    console.error({ err })
})
}

main()