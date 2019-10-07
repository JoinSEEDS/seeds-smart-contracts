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

module.exports = compile