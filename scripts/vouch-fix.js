const { eos, names } = require('./helper')
const { accounts } = names
const fs = require('fs').promises
const { join } = require('path')

const args = process.argv.slice(2)
const useSubRep = args.length > 0 ? args[0] == 'true' : false

const fileExists = async path => !!(await fs.stat(path).catch(e => false))

async function loadFixedFile () {
  const path = join(__dirname, 'totals_fixed.txt')
  
  if (await fileExists(path)) {
    const fileData = await fs.readFile(path, 'utf8')
    return JSON.parse(fileData)
  }

  return {}
}

async function getOldVouchPoints (user) {
  const oldVouchEntries = await eos.getTableRows({
    code: accounts,
    scope: user,
    table: 'vouch',
    json: true,
    limit: 10000
  })
  let total = 0
  for (const oldEntry of oldVouchEntries.rows) {
    total += oldEntry.reps
  }
  return total
}

async function subRep (user, amount) {
  console.log('sub rep for user', user + ',', 'rep amount:', amount)
  if (useSubRep) {
    await eos.transaction({
      actions: [{
        account: accounts,
        name: 'subrep',
        authorization: [{
          actor: accounts,
          permission: 'active',
        }],
        data: {
          user,
          amount
        }
      }]
    })
  }
}

async function main () {
  
  const path = join(__dirname, 'totals_fix_log.txt')
  const fileData = await fs.readFile(path, 'utf8')

  const vouchTotals = (JSON.parse(fileData)).rows
  const fixedUsers = await loadFixedFile()

  const MAX_REP = 50
  
  for (const vouchTotal of vouchTotals) {

    try {

      // validate the user is not in the done files
      if (fixedUsers[vouchTotal.account]) {
        console.log('skipped user:', vouchTotal.account, ' as the user has been processed before')
        continue
      }

      console.log('processing user:', vouchTotal.account)

      // get its entry in the old vouch table
      const oldTotal = await getOldVouchPoints(vouchTotal.account)

      // compare the result with the vouchTotal info
      const delta = oldTotal + vouchTotal.total_rep_points - MAX_REP
      
      // remove rep if needed
      if (delta > 0) {
        await subRep(vouchTotal.account, delta)
      }

      // save the user as processed
      fixedUsers[vouchTotal.account] = {
        delta,
        old_rep_total: oldTotal,
        new_vouch_total_rep_points: vouchTotal.total_rep_points
      }

    } catch (err) {
      console.log('An error ocurred:', err)
    }

  }

  await fs.writeFile(join(__dirname, 'totals_fixed.txt'), JSON.stringify(fixedUsers, null, 2))

}


main()

