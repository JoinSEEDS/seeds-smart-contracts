const { describe } = require("riteway")
const { eos, encodeName, names, getTableRows, isLocal, initContracts, createKeypair } = require("../scripts/helper")

const { accounts, harvest, region, token, firstuser, seconduser, thirduser, bank, settings, history, fourthuser } = names

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

const randomAccountNamergn = () => {
  let length = 8
  var result           = '';
  var characters       = 'abcdefghijklmnopqrstuvwxyz1234';
  var charactersLength = characters.length;
  for ( var i = 0; i < length; i++ ) {
     result += characters.charAt(Math.floor(Math.random() * charactersLength));
  }
  return result + ".rdc";
}

describe("regions general", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, region, token, settings })

  const keypair = await createKeypair();

  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('region reset')
  await contracts.region.reset({ authorization: `${region}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset token stats')
  await contracts.token.resetweekly({ authorization: `${token}@active` })

  console.log('configure - 1 Seeds feee')
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })

  console.log('configure - rdc.cit')
  await contracts.settings.configure("rdc.cit", 2, { authorization: `${settings}@active` })

  const statusInactive = 'inactive'
  const statusActive = 'active'

  const checkStatus = async (rgn, status, given, should) => {
    const regions = await getTableRows({
      code: region,
      scope: region,
      table: 'regions',
      json: true
    })
    assert({
      given,
      should,
      actual: regions.rows.filter(r => r.id == rgn)[0].status,
      expected: status
    })
  }

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('transfer fee for a region')
  await contracts.token.transfer(firstuser, region, "1.0000 SEEDS", "Initial supply", { authorization: `${firstuser}@active` })

  const rgnname = randomAccountNamergn()

  const getMembers = async () => {
    return await getTableRows({
      code: region,
      scope: region,
      table: 'members',
      json: true
    })
  }
  const getRoles = async () => {
    return await getTableRows({
      code: region,
      scope: rgnname,
      table: 'roles',
      json: true
    })
  }

  console.log('create a region named '+rgnname)

  await contracts.region.create(
    firstuser, 
    rgnname, 
    'test rgn region',
    '{lat:0.0111,lon:1.3232}', 
    1.1, 
    1.23, 
    keypair.public, 
    { authorization: `${firstuser}@active` })

    const regions = await getTableRows({
      code: region,
      scope: region,
      table: 'regions',
      json: true
    })
    //console.log("regions "+JSON.stringify(regions, null, 2))
  
    const initialMembers = await getMembers()
    
    var hasNewDomain = false
    try {
      const accountInfo = await eos.getAccount(rgnname)
      hasNewDomain = true;
    } catch {
  
    }

  await checkStatus(rdcname, statusInactive, 'region created', 'have the correct status')
  
  console.log('join a region')
  await contracts.region.join(rgnname, seconduser, { authorization: `${seconduser}@active` })

  const members = await getMembers()
  //console.log("members "+JSON.stringify(members, null, 2))
  await checkStatus(rdcname, statusActive, `${seconduser} joined`, 'have the correct status')

  console.log('leave a region')
  await contracts.region.leave(rgnname, seconduser, { authorization: `${seconduser}@active` })

  const membersAfter = await getMembers()
  await checkStatus(rdcname, statusInactive, `${seconduser} left`, 'have the correct status')

  //console.log("membersAfter "+JSON.stringify(membersAfter, null, 2))

  console.log('remove a member')
  await contracts.region.join(rgnname, thirduser, { authorization: `${thirduser}@active` })
  await contracts.region.removemember(rgnname, firstuser, thirduser, { authorization: `${firstuser}@active` })

  const membersAfterRemove = await getMembers()
  //console.log("membersAfterRemove "+JSON.stringify(membersAfterRemove, null, 2))
  await checkStatus(rdcname, statusInactive, `${thirduser} joined`, 'have the correct status')

  const admin = seconduser

  let delayWorks = true
  try {
    await contracts.region.join(rgnname, seconduser, { authorization: `${seconduser}@active` })
    delayWorks = false
  } catch (err) {}

  console.log('configure rgn.vote.del to 0')
  await contracts.settings.conffloat("rgn.vote.del", 0, { authorization: `${settings}@active` })
  await sleep(1000)


  console.log('add role')
  await contracts.region.join(rgnname, seconduser, { authorization: `${seconduser}@active` })

  await contracts.region.addrole(rgnname, firstuser, admin, "admin", { authorization: `${firstuser}@active` })

  const roles = await getRoles()
  //console.log("roles "+JSON.stringify(roles, null, 2))

  console.log('remove role')
  await contracts.region.removerole(rgnname, firstuser, admin, { authorization: `${firstuser}@active` })
  const rolesAfter = await getRoles()
  //console.log("rolesAfter "+JSON.stringify(rolesAfter, null, 2))

  await contracts.region.join(rgnname, fourthuser, { authorization: `${fourthuser}@active` })
  await contracts.region.addrole(rgnname, firstuser, fourthuser, "admin", { authorization: `${firstuser}@active` })
  await contracts.region.leaverole(rgnname, fourthuser, { authorization: `${fourthuser}@active` })
  const rolesAfter2 = await getRoles()

  console.log('admin')
  await contracts.region.addrole(rgnname, firstuser, seconduser, "admin", { authorization: `${firstuser}@active` })
  await contracts.region.join(rgnname, thirduser, { authorization: `${thirduser}@active` })

  console.log('admin removes member')
  const membersBeforeAdminRemove = await getMembers()
  await contracts.region.removemember(rgnname, seconduser, thirduser, { authorization: `${seconduser}@active` })
  const membersAfterAdminRemove = await getMembers()
  //console.log("membersAfterAdminRemove "+JSON.stringify(membersAfterAdminRemove, null, 2))

  var cantremove = true
  try {
    await contracts.region.removemember(rgnname, seconduser, firstuser, { authorization: `${seconduser}@active` })
    cantremove = false
  } catch { }
  var cantremovefounder = true
  try {
    await contracts.region.removemember(rgnname, firstuser, firstuser, { authorization: `${seconduser}@active` })
    cantremovefounder = false
  } catch { }
  
  var cantaddrole = true
  try {
    await contracts.region.addrole(rgnname, seconduser, thirduser, "founder")
    cantaddrole = false
  } catch { }

  var cantsetfounder = true
  try {
    await contracts.region.setfounder(rgnname, seconduser, seconduser, { authorization: `${seconduser}@active` })
    cantsetfounder = false
  } catch { }

  await contracts.region.setfounder(rgnname, firstuser, seconduser, { authorization: `${firstuser}@active` })
  const regionsFounder = await getTableRows({
    code: region,
    scope: region,
    table: 'regions',
    json: true
  })

  assert({
    given: 'change founder',
    should: 'have different foudner',
    actual: regionsFounder.rows[0].founder,
    expected: seconduser
  })
  



assert({
  given: 'create region',
  should: 'have region',
  actual: regions.rows.length,
  expected: 1
})

assert({
  given: 'create region',
  should: 'have member',
  actual: initialMembers.rows.length,
  expected: 1
})

assert({
  given: 'join member',
  should: 'have one more member',
  actual: members.rows.length,
  expected: 2
})

assert({
  given: 'leave',
  should: 'have one less member',
  actual: membersAfter.rows.length,
  expected: 1
})

assert({
  given: 'member removed',
  should: 'have one less member',
  actual: membersAfterRemove.rows.length,
  expected: 1
})

assert({
  given: 'add role',
  should: 'have role',
  actual: roles.rows.map(({account})=>account),
  expected: [
    firstuser,
    seconduser,
]
})
assert({
  given: 'remove role',
  should: 'have role',
  actual: rolesAfter.rows.map(({account})=>account),
  expected: [
    firstuser,
  ]
})

assert({
  given: 'remove admin',
  should: 'have one less member',
  actual: membersBeforeAdminRemove.rows.length - membersAfterAdminRemove.rows.length,
  expected: 1
})

assert({
  given: 'not allowed to remove member',
  should: 'be true',
  actual: cantremove,
  expected: true
})

assert({
  given: 'not allowed to add role',
  should: 'be true',
  actual: cantaddrole,
  expected: true
})

assert({
  given: 'not allowed to set founder',
  should: 'be true',
  actual: cantsetfounder,
  expected: true
})

assert({
  given: 'user left',
  should: 'have to wait for the delay to end',
  actual: delayWorks,
  expected: true
})


})

describe("regions Test Delete", async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const contracts = await initContracts({ accounts, region, token, settings })

  const keypair = await createKeypair();

  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('region reset')
  await contracts.region.reset({ authorization: `${region}@active` })

  console.log('accounts reset')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('configure - 1 Seeds feee')
  await contracts.settings.configure("region.fee", 10000 * 1, { authorization: `${settings}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'first user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'second user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser user', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'second user', 'individual', { authorization: `${accounts}@active` })

  console.log('transfer fee for a region')
  await contracts.token.transfer(firstuser, region, "1.0000 SEEDS", "test", { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, region, "1.0000 SEEDS", "test", { authorization: `${seconduser}@active` })

  const rgnname = "rgn1.rgn"
  const rgnname2 = "rgn2.rgn"

  const getMembers = async () => {
    return await getTableRows({
      code: region,
      scope: region,
      table: 'members',
      json: true
    })
  }
  
  const getRoles = async (br) => {
    return await getTableRows({
      code: region,
      scope: br,
      table: 'roles',
      json: true
    })
  }

  console.log('create a region named '+rgnname)

  await contracts.region.create(
    firstuser, 
    rgnname, 
    'test rgn region',
    '{lat:0.0111,lon:1.3232}', 
    1.1, 
    1.23, 
    keypair.public, 
    { authorization: `${firstuser}@active` })

    lat = 39.0894
    lon = 1.4539

    console.log('create a second region named '+rgnname2)

    await contracts.region.create(
      seconduser, 
      rgnname2, 
      'test rgn region 1',
      '{lat:0.0111,lon:1.3232}', 
      1.1, 
      1.23, 
      keypair.public, 
      { authorization: `${seconduser}@active` })
  
  
  console.log('join a region')
  await contracts.region.join(rgnname2, fourthuser, { authorization: `${fourthuser}@active` })
  await contracts.region.join(rgnname, thirduser, { authorization: `${thirduser}@active` })

  console.log('add role')
  await contracts.region.addrole(rgnname, firstuser, thirduser, "admin", { authorization: `${firstuser}@active` })
  await contracts.region.addrole(rgnname2, seconduser, fourthuser, "admin", { authorization: `${seconduser}@active` })

  const membersBefore = await getMembers()
  const roles1before = await getRoles(rgnname)
  const roles2before = await getRoles(rgnname2)

  // console.log("membersBefore "+JSON.stringify(membersBefore, null, 2))
  // console.log("roles1before "+JSON.stringify(roles1before, null, 2))
  // console.log("roles2before "+JSON.stringify(roles2before, null, 2))

  await contracts.region.removebr(rgnname2, { authorization: `${region}@active` })

  const roles1After = await getRoles(rgnname)
  const roles2After = await getRoles(rgnname2)
  const membersAfter = await getMembers()

  // console.log("membersAfter "+JSON.stringify(membersAfter, null, 2))
  // console.log("roles2After "+JSON.stringify(roles2After, null, 2))
  // console.log("roles1After "+JSON.stringify(roles1After, null, 2))

  assert({
    given: 'remove region 2',
    should: 'region 1 unaffected',
    actual: roles1before.rows.length == roles1After.rows.length,
    expected: true
  })

  assert({
    given: 'remove region 2',
    should: 'region 2 removed roles',
    actual: roles2before.rows.length - roles2After.rows.length,
    expected: 2
  })
    
assert({
  given: 'remove region 2',
  should: 'have less members',
  actual: membersBefore.rows.length - membersAfter.rows.length,
  expected: 2
})


})
