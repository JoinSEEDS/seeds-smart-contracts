const { exec } = require('child_process')

const command = ({ contract, source }) => {
    const volume = '/home/sevenflash/apps/seeds-contracts'

    const cmd = `docker run --rm --name eosio.cdt_v1.6.1 --volume ${volume}:/project -w /project eostudio/eosio.cdt:v1.6.1 /bin/bash -c "eosio-cpp -abigen -I ./include -contract ${contract} -o ./artifacts/${contract}.wasm ${source}"`
 
    return cmd
}

const compile = (contract) => {
  const source = `./src/seeds.${contract}.cpp`

  return new Promise((resolve, reject) => {
      exec(command({ contract, source }), (error, stdout, stderr) => {
        if (error) return reject(error)

        resolve()
      })
  })
}

module.exports = compile