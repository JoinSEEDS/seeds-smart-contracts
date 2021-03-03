/*
 * JavaScript helper class for onboarding contract
 * 
 * Usage: Import createInviteSecretAndHash method, use it to create both secret and hash
 * which work with the onboarding contract.
 * 
 * Specifically the onboarding contracts requires a 64 byte hex string as secret. We use eos-ecc to generate
 * this securely and randomly.
 * 
 * The hasn also is generated with eosjs-ecc - for compatibility with contract code, it needs to use this
 * exact way for generating a sha256. 
 * 
*/

const Eos = require('./eosjs-port')
const ecc = Eos.getEcc()

const ramdom64ByteHexString = async () => {
  let privateKey = await ecc.randomKey(undefined, {
    secureEnv: true
  })
  const encoded = Buffer.from(privateKey).toString('hex').substring(0, 64); 
  return encoded
}

const fromHexString = hexString => new Uint8Array(hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16)))

const createInviteSecretAndHash = async () => {
  const secret = await ramdom64ByteHexString()
  const hash = ecc.sha256(fromHexString(secret)).toString('hex')
  return {
    secret,
    hash
  }
}

const main = async () => {
    console.log("secret: "+JSON.stringify(await createInviteSecretAndHash(), null, 2))
}

main()

module.exports = {createInviteSecretAndHash}