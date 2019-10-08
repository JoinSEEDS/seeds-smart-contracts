const { exec } = require('child_process')

const test = (contract) => {
  return new Promise((resolve, reject) => {
    exec(`npx riteway test/${contract}.test.js | npx tap-nirvana`, (err, stderr, stdout) => {
      console.error(stderr)
      console.log(stdout)
      resolve()
    })
  })
}

module.exports = test