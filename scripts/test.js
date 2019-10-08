const { exec } = require('child_process')

const test = (contract) => {
  return new Promise((resolve, reject) => {
    exec(`node test ../test/${contract}.test.js`, (err) => {
      if (err) return reject(err)
      
      resolve()
    })
  })
}

module.exports = test