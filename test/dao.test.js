const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, isLocal } = require('../scripts/helper')
const { should, assert } = require('chai')

const { 
  dao, token, settings, accounts, harvest, proposals, referendums, firstuser, seconduser, thirduser, fourthuser,
  alliancesbank, campaignbank, escrow
} = names

const scopes = ['alliance', proposals, 'milestone', referendums]


const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

function formatSpecialAttributes (proposals) {
  const props = []

  for (const prop of proposals) {
    const r = { ...prop }
    delete r.special_attributes

    for (const attr of prop.special_attributes) {
      r[attr.key] = attr.value[1]
    }

    props.push(r)
  }

  return props
}

const createReferendum = async (contract, creator, settingName, newValue, title, summary, description, image, url) => {
  await contract.create([
    { key: 'type', value: ['name', 'r.setting'] },
    { key: 'creator', value: ['name', creator] },
    { key: 'setting_name', value: ['name', settingName] },
    { key: 'title', value: ['string', title] },
    { key: 'summary', value: ['string', summary] },
    { key: 'description', value: ['string', description] },
    { key: 'image', value: ['string', image] },
    { key: 'url', value: ['string', url] },
    { key: 'new_value', value: newValue },
    { key: 'test_cycles', value: ['uint64', 1] },
    { key: 'eval_cycles', value: ['uint64', 3] }
  ], { authorization: `${creator}@active` })
}

describe('Referendums Settings', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const testSetting = 'testsetting'
  const testSettingFloat = 'ftestsetting'

  const uintSettingTable = 'config'
  const floatSettingTable = 'configfloat'

  const testSettingPrevVal = 1
  const testSettingFloatPrevVal = 1.2

  const testSettingNewValue = 10
  const testSettingFloatNewValue = 2.2

  const minStake = 1111

  const highImpact = 'high'
  const medImpact = 'med'
  const lowImpact = 'low'

  const users = [firstuser, seconduser, thirduser, fourthuser]

  const contracts = await initContracts({ dao, token, settings, accounts })

  console.log('reset referendums')
  await contracts.dao.reset({ authorization: `${dao}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  const createReferendum = async (creator, settingName, newValue, title, summary, description, image, url) => {
    await contracts.dao.create([
      { key: 'type', value: ['name', 'r.setting'] },
      { key: 'creator', value: ['name', creator] },
      { key: 'setting_name', value: ['name', settingName] },
      { key: 'title', value: ['string', title] },
      { key: 'summary', value: ['string', summary] },
      { key: 'description', value: ['string', description] },
      { key: 'image', value: ['string', image] },
      { key: 'url', value: ['string', url] },
      { key: 'new_value', value: newValue },
      { key: 'test_cycles', value: ['uint64', 1] },
      { key: 'eval_cycles', value: ['uint64', 3] }
    ], { authorization: `${creator}@active` })
  }

  const checkReferendums = async (
    expectedRefs,
    expectedAuxs,
    given='on period ran',
    should='have the correct entries'
  ) => {

    const referendumsTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'proposals',
      json: true
    })
    console.log(referendumsTable.rows)

    assert({
      given,
      should,
      actual: referendumsTable.rows.map(r => {
        return {
          proposal_id: r.proposal_id,
          favour: r.favour,
          against: r.against,
          staked: r.staked,
          status: r.status,
          stage: r.stage,
          age: r.age,
        }
      }),
      expected: expectedRefs
    })

    const refauxTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'propaux',
      json: true
    })
    console.log(formatSpecialAttributes(refauxTable.rows))

    assert({
      given,
      should,
      actual: formatSpecialAttributes(refauxTable.rows).map(r => {
        return {
          proposal_id: r.proposal_id,
          is_float: r.is_float,
          new_value: r.is_float ? parseFloat(r.new_value) : parseInt(r.new_value),
          previous_value: r.is_float ? parseFloat(r.previous_value) : parseInt(r.previous_value),
          setting_name: r.setting_name,
          cycles_per_status: r.cycles_per_status
        }
      }),
      expected: expectedAuxs
    })

  }

  const voteReferendum = async (referendumId, number=null, favour=true) => {

    const vote = favour ? contracts.dao.favour : contracts.dao.against
    number = number == null ? users.length : number

    for (let i = 0; i < number; i++) {
      await vote(users[i], referendumId, 2, { authorization: `${users[i]}@active` })
    }

  }

  const revertVote = async (referendumId, number=null) => {

    number = number == null ? users.length : number

    for (let i = 0; i < number; i++) {
      await contracts.dao.revertvote(users[i], referendumId, { authorization: `${users[i]}@active` })
    }

  }

  const checkSettingValue = async ({ settingName, expectedValue, tableName, given, should }) => {
    const settingsTable = await getTableRows({
      code: settings,
      scope: settings,
      table: tableName,
      json: true,
      limit: 200
    })

    const settingRow = settingsTable.rows.filter(r => r.param === settingName)[0]

    assert({
      given,
      should,
      actual: Math.abs(settingRow.value - expectedValue) <= 0.001,
      expected: true
    })
  }

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('insert test settings')
  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', highImpact, { authorization: `${settings}@active` })
  await contracts.settings.conffloatdsc(testSettingFloat, 1.2, 'test description 2', medImpact, { authorization: `${settings}@active` })

  console.log('join users')
  await Promise.all(users.map(user => contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })))
  await Promise.all(users.map(user => contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })))
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('create referendum')
  await createReferendum(firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(seconduser, testSettingFloat, ['float64', testSettingFloatNewValue], 'title 2', 'summary 2', 'description 2', 'image 2', 'url 2')

  console.log('update referendum')
  await contracts.dao.update([
    { key: 'proposal_id', value: ['uint64', 0] },
    { key: 'setting_name', value: ['name', testSetting] },
    { key: 'title', value: ['string', 'title updated'] },
    { key: 'summary', value: ['string', 'summary updated'] },
    { key: 'description', value: ['string', 'description updated'] },
    { key: 'image', value: ['string', 'image updated'] },
    { key: 'url', value: ['string', 'url updated'] },
    { key: 'new_value', value: ['uint64', testSettingNewValue] },
    { key: 'test_cycles', value: ['uint64', 1] },
    { key: 'eval_cycles', value: ['uint64', 4] }
  ], { authorization: `${firstuser}@active` })

  console.log('delete referendum')
  await contracts.dao.cancel([{ key: 'proposal_id', value: ['uint64', 1] }], { authorization: `${firstuser}@active` })

  const refTables = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    limit: 200
  })

  assert({
    given: 'referendums crated',
    should: 'have the correct entries',
    actual: refTables.rows.map(r => {
      delete r.created_at
      return r
    }),
    expected: [
      {
        proposal_id: 0,
        favour: 0,
        against: 0,
        staked: '0.0000 SEEDS',
        creator: firstuser,
        title: 'title updated',
        summary: 'summary updated',
        description: 'description updated',
        image: 'image updated',
        url: 'url updated',
        status: 'open',
        stage: 'staged',
        type: 'r.setting',
        last_ran_cycle: 0,
        age: 0
      },
      {
        proposal_id: 2,
        favour: 0,
        against: 0,
        staked: '0.0000 SEEDS',
        creator: seconduser,
        title: 'title 2',
        summary: 'summary 2',
        description: 'description 2',
        image: 'image 2',
        url: 'url 2',
        status: 'open',
        stage: 'staged',
        type: 'r.setting',
        last_ran_cycle: 0,
        age: 0
      }
    ]
  })

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('vote favour for first referendum')
  await voteReferendum(0)

  console.log('staking for second referendum')
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '2', { authorization: `${seconduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkReferendums(
    [
      {
        proposal_id: 0,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'test',
        stage: 'active',
        age: 1,
      },
      {
        proposal_id: 2,
        favour: 0,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'voting',
        stage: 'active',
        age: 0
      }
    ], [
      {
        proposal_id: 0,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 2,
        is_float: 1,
        new_value: testSettingFloatNewValue,
        previous_value: testSettingFloatPrevVal,
        setting_name: testSettingFloat,
        cycles_per_status: '1,1,3'
      }
    ]
  )

  await checkSettingValue({ 
    settingName: testSetting, 
    expectedValue: testSettingPrevVal, 
    tableName: uintSettingTable, 
    given: 'referendum in test stage', 
    should: 'not modify the value of the setting' 
  })

  console.log('vote favour for second referendum')
  await voteReferendum(2, 3)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkReferendums(
    [
      {
        proposal_id: 0,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 2,
      },
      {
        proposal_id: 2,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'test',
        stage: 'active',
        age: 1
      }
    ], [
      {
        proposal_id: 0,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 2,
        is_float: 1,
        new_value: testSettingFloatNewValue,
        previous_value: testSettingFloatPrevVal,
        setting_name: testSettingFloat,
        cycles_per_status: '1,1,3'
      }
    ]
  )

  await checkSettingValue({ 
    settingName: testSetting, 
    expectedValue: testSettingNewValue, 
    tableName: uintSettingTable, 
    given: 'referendum in eval stage', 
    should: 'modify the value of the setting' 
  })

  for (let i = 0; i < 3; i++) {
    console.log('running onperiod')
    await contracts.dao.onperiod({ authorization: `${dao}@active` })
    await sleep(2000)
  }

  await checkReferendums(
    [
      {
        proposal_id: 0,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 5,
      },
      {
        proposal_id: 2,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 4
      }
    ], [
      {
        proposal_id: 0,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 2,
        is_float: 1,
        new_value: testSettingFloatNewValue,
        previous_value: testSettingFloatPrevVal,
        setting_name: testSettingFloat,
        cycles_per_status: '1,1,3'
      }
    ]
  )

  await checkSettingValue({ 
    settingName: testSettingFloat, 
    expectedValue: testSettingFloatNewValue, 
    tableName: floatSettingTable, 
    given: 'referendum in eval stage', 
    should: 'modify the value of the setting' 
  })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkReferendums(
    [
      {
        proposal_id: 0,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'passed',
        stage: 'done',
        age: 6,
      },
      {
        proposal_id: 2,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'passed',
        stage: 'done',
        age: 5
      }
    ], [
      {
        proposal_id: 0,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 2,
        is_float: 1,
        new_value: testSettingFloatNewValue,
        previous_value: testSettingFloatPrevVal,
        setting_name: testSettingFloat,
        cycles_per_status: '1,1,3'
      }
    ]
  )

  console.log('failing referendums')

  console.log('create referendum')
  await createReferendum(firstuser, testSetting, ['uint64', 5], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(seconduser, testSettingFloat, ['float64', 0.2], 'title 2', 'summary 2', 'description 2', 'image 2', 'url 2')
  await createReferendum(thirduser, testSettingFloat, ['float64', 22.2], 'title 3', 'summary 3', 'description 3', 'image 3', 'url 3')
  await createReferendum(thirduser, testSettingFloat, ['float64', 3.2], 'title 4', 'summary 4', 'description 4', 'image 4', 'url 4')

  console.log('staking for referendums')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '3', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '4', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, dao, `${minStake}.0000 SEEDS`, '5', { authorization: `${thirduser}@active` })
  await contracts.token.transfer(thirduser, dao, `${minStake}.0000 SEEDS`, '6', { authorization: `${thirduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await voteReferendum(3)
  await voteReferendum(4)
  await voteReferendum(5, 4, false)
  await voteReferendum(6)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('change trust for referendum 1')
  await revertVote(4, 3)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkSettingValue({ 
    settingName: testSetting, 
    expectedValue: 5, 
    tableName: uintSettingTable, 
    given: 'referendum in eval stage', 
    should: 'modify the value of the setting' 
  })

  await checkSettingValue({ 
    settingName: testSettingFloat, 
    expectedValue: 3.2, 
    tableName: floatSettingTable, 
    given: 'referendum in eval stage', 
    should: 'modify the value of the setting' 
  })

  console.log('change trust for referendum 3 and 6')
  await revertVote(3, 3)
  await revertVote(6, 3)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  const refTables2 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    limit: 200
  })

  assert({
    given: 'referendums failing',
    should: 'have the correct status',
    actual: refTables2.rows.filter(r => r.proposal_id > 2).map(r => {
      return {
        proposal_id: r.proposal_id,
        status: r.status,
        stage: r.stage
      }
    }),
    expected: Array.from(Array(4).keys()).map(key => {
      return {
        proposal_id: key + 3,
        status: 'rejected',
        stage: 'done'
      }
    })
  })

  await checkSettingValue({ 
    settingName: testSetting, 
    expectedValue: testSettingNewValue, 
    tableName: uintSettingTable, 
    given: 'referendum has failed', 
    should: 'return the setting to its old value' 
  })

  await checkSettingValue({ 
    settingName: testSettingFloat, 
    expectedValue: testSettingFloatNewValue, 
    tableName: floatSettingTable, 
    given: 'referendum has failed', 
    should: 'return the setting to its old value'
  })

})


async function resetContracts () {
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  console.log('reset dao')
  await contracts.dao.reset({ authorization: `${dao}@active` })
  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })
  
  console.log('join users')
  await Promise.all(users.map(user => contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })))
  await Promise.all(users.map(user => contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })))
}

async function getVoice (account) {
  const voice = []
  for (const s of scopes) {
    const voiceTable = await getTableRows({
      code: dao,
      scope: s,
      table: 'voice',
      json: true,
      lower_bound: account,
      limit: 1
    })
    if (voiceTable.rows.length > 0) {
      voice.push({
        scope: s,
        ...voiceTable.rows[0]
      })
    }
  }
  return voice
}

describe('Participants and Actives', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  const testSetting = 'testsetting'
  const minStake = 1111

  const checkParticipants = async (expected) => {
    const participantsTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'participants',
      json: true
    })
  
    assert({
      given: 'users voted',
      should: 'be in the participants table',
      actual: participantsTable.rows,
      expected
    })
  }

  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', 'high', { authorization: `${settings}@active` })

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create referendum')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await contracts.dao.favour(firstuser, 0, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.against(seconduser, 0, 5, { authorization: `${seconduser}@active` })
  await contracts.dao.neutral(thirduser, 0, { authorization: `${thirduser}@active` })

  await checkParticipants([
    { account: firstuser, nonneutral: 1, count: 1 },
    { account: seconduser, nonneutral: 1, count: 1 },
    { account: thirduser, nonneutral: 0, count: 1 }
  ])

  const activesTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'actives',
    json: true
  })

  const now = parseInt(Date.now() / 1000)
  const actualActives = activesTable.rows.map(a => {
    return {
      account: a.account,
      timestamp: Math.abs(now - a.timestamp) <= 2
    }
  })

  assert({
    given: 'users voted',
    should: 'be in the actives table',
    actual: actualActives,
    expected: [
      { account: firstuser, timestamp: true },
      { account: seconduser, timestamp: true },
      { account: thirduser, timestamp: true }
    ]
  })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(3000)

  await checkParticipants([])

})

describe('Voting', async assert => {
  
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  const testSetting = 'testsetting'
  const minStake = 1111

  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', 'high', { authorization: `${settings}@active` })

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create referendum')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('voting for proposal with scope referendums')
  await contracts.dao.favour(firstuser, 0, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.against(seconduser, 0, 5, { authorization: `${seconduser}@active` })
  await contracts.dao.neutral(thirduser, 0, { authorization: `${thirduser}@active` })

  const actualVoices = []
  
  for (const user of users) {
    const voices = await getVoice(user)
    actualVoices.push(voices)
  }
  
  assert({
    given: 'vote done for a referendum',
    should: 'have the correct scope',
    actual: actualVoices.map(av => av.filter(a => a.scope === referendums)),
    expected: [
      [{ scope: 'rules.seeds', account: 'seedsuseraaa', balance: 89 }],
      [{ scope: 'rules.seeds', account: 'seedsuserbbb', balance: 94 }],
      [{ scope: 'rules.seeds', account: 'seedsuserccc', balance: 99 }],
      [{ scope: 'rules.seeds', account: 'seedsuserxxx', balance: 99 }]
    ]
  })

  const voiceTable = await getTableRows({
    code: dao,
    scope: 0,
    table: 'votes',
    json: true
  })

  assert({
    given: 'vote done for a referendum',
    should: 'have the votes',
    actual: voiceTable.rows,
    expected: [
      { proposal_id: 0, account: firstuser, amount: 10, favour: 1 },
      { proposal_id: 0, account: seconduser, amount: 5, favour: 0 },
      { proposal_id: 0, account: thirduser, amount: 0, favour: 0 }
    ]
  })

  const votedProp = await getTableRows({
    code: dao,
    scope: 2,
    table: 'cycvotedprps',
    json: true
  })

  assert({
    given: 'vote done for a referendum',
    should: 'have an entry in the voted proposal table',
    actual: votedProp.rows.map(v => v.proposal_id),
    expected: [0]
  })

})

describe('Voice Update', async assert => {
  
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  
  await Promise.all(users.map((user, index) => contracts.harvest.testupdatecs(user, (index + 1) * 20, { authorization: `${harvest}@active` })))
  await Promise.all(users.map(user => contracts.dao.changetrust(user, true, { authorization: `${dao}@active` })))
  await contracts.dao.updatevoices({ authorization: `${dao}@active` })
  await sleep(2000)
  
  const actualVoices = []
  
  for (const user of users) {
    const voices = await getVoice(user)
    actualVoices.push(voices)
  }
  
  const expectedVoices = []
  let index = 0
  
  for (const user of users) {
    index += 1
    expectedVoices.push(scopes.map(scope => {
      return {
        scope,
        account: user,
        balance: index * 20
      }
    }))
  }
  
  assert({
    given: 'updatevoices called',
    should: 'have the correct voice balances',
    actual: actualVoices,
    expected: expectedVoices
  })
})

describe('Voice Decay', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  
  await Promise.all(users.map((user, index) => contracts.harvest.testupdatecs(user, (index + 1) * 20, { authorization: `${harvest}@active` })))
  await Promise.all(users.map(user => contracts.dao.changetrust(user, true, { authorization: `${dao}@active` })))

  console.log('change batch size')
  await contracts.settings.configure('batchsize', 3, { authorization: `${settings}@active` })

  console.log('change decaytime')
  await contracts.settings.configure('decaytime', 30, { authorization: `${settings}@active` })

  console.log('propdecaysec')
  await contracts.settings.configure('propdecaysec', 5, { authorization: `${settings}@active` })

  console.log('vdecayprntge = 15%')
  await contracts.settings.configure('vdecayprntge', 15, { authorization: `${settings}@active` })

  const testVoiceDecay = async (expectedVoices) => {

    await contracts.dao.decayvoices({ authorization: `${dao}@active` })
    await sleep(2000)

    const actualVoices = []
  
    for (const user of users) {
      const voices = await getVoice(user)
      actualVoices.push(voices)
    }

    assert({
      given: 'voice decay ran',
      should: 'decay voices properly',
      actual: actualVoices.map(av => av.map(a => a.balance)),
      expected: expectedVoices.map(v => Array.from(Array(scopes.length).keys()).map(a => v)  )
    })

  }

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  await sleep(4000)

  await testVoiceDecay([20, 40, 60, 80])
  await sleep(10000)
  await testVoiceDecay([20, 40, 60, 80])
  await sleep(15000)
  await testVoiceDecay([17, 34, 51, 68])
  await sleep(1000)
  await testVoiceDecay([17, 34, 51, 68])
  await sleep(4000)
  await testVoiceDecay([14, 28, 43, 57])
  await sleep(2000)

})

describe('Voice Delegation', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })

  await Promise.all(users.map((user, index) => contracts.harvest.testupdatecs(user, (index + 1) * 20, { authorization: `${harvest}@active` })))
  await Promise.all(users.map(user => contracts.dao.changetrust(user, true, { authorization: `${dao}@active` })))

  const checkVoices = async (expectedVoices) => {

    const actualVoices = []
  
    for (const user of users) {
      const voices = await getVoice(user)
      actualVoices.push(voices)
    }

    assert({
      given: 'voice delegated',
      should: 'have the correct balances after voting',
      actual: actualVoices.map(av => av.filter(a => a.scope === referendums)[0]),
      expected: expectedVoices
    })

  }

  const testSetting = 'testsetting'
  const minStake = 1111

  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', 'high', { authorization: `${settings}@active` })

  const createReferendum = async (creator, settingName, newValue, title, summary, description, image, url) => {
    await contracts.dao.create([
      { key: 'type', value: ['name', 'r.setting'] },
      { key: 'creator', value: ['name', creator] },
      { key: 'setting_name', value: ['name', settingName] },
      { key: 'title', value: ['string', title] },
      { key: 'summary', value: ['string', summary] },
      { key: 'description', value: ['string', description] },
      { key: 'image', value: ['string', image] },
      { key: 'url', value: ['string', url] },
      { key: 'new_value', value: newValue },
      { key: 'test_cycles', value: ['uint64', 1] },
      { key: 'eval_cycles', value: ['uint64', 3] }
    ], { authorization: `${creator}@active` })
  }

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create referendum')
  await createReferendum(firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('delegate voice')
  await contracts.dao.delegate(seconduser, firstuser, referendums, { authorization: `${seconduser}@active` })
  await contracts.dao.delegate(thirduser, firstuser, referendums, { authorization: `${thirduser}@active` })
  await contracts.dao.delegate(fourthuser, thirduser, referendums, { authorization: `${fourthuser}@active` })

  console.log('voting')
  await contracts.dao.favour(firstuser, 0, 5, { authorization: `${firstuser}@active` })
  await sleep(5000)

  await checkVoices([
    { scope: referendums, account: firstuser, balance: 15 },
    { scope: referendums, account: seconduser, balance: 30 },
    { scope: referendums, account: thirduser, balance: 45 },
    { scope: referendums, account: fourthuser, balance: 60 }
  ])

  console.log('Undelegate voice')
  await contracts.dao.undelegate(seconduser, referendums, { authorization: `${seconduser}@active` })

  console.log('change opinion, revert vote')

  const proposalsTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    limit: 200
  })

  console.log(proposalsTable)

  assert({
    given: 'delegated vote',
    should: 'have the correct favour and against',
    actual: [proposalsTable.rows[0].favour, proposalsTable.rows[0].against],
    expected: [50, 0]
  })

  await contracts.dao.revertvote(firstuser, 0, { authorization: `${firstuser}@active` })
  await sleep(5000)

  await checkVoices([
    { scope: referendums, account: firstuser, balance: 15 },
    { scope: referendums, account: seconduser, balance: 30 },
    { scope: referendums, account: thirduser, balance: 45 },
    { scope: referendums, account: fourthuser, balance: 60 }
  ])

  const proposalsTable2 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    limit: 200
  })

  console.log('\n')
  console.log(proposalsTable2)

  assert({
    given: 'delegated vote',
    should: 'have the correct favour and against',
    actual: [proposalsTable2.rows[0].favour, proposalsTable2.rows[0].against],
    expected: [10, 40]
  })

})


const createProp = async (contract, creator, type, title, summary, description, image, url, fund, quantity, options) => {
  await contract.create([
    { key: 'type', value: ['name', type] },
    { key: 'creator', value: ['name', creator] },
    { key: 'title', value: ['string', title] },
    { key: 'summary', value: ['string', summary] },
    { key: 'description', value: ['string', description] },
    { key: 'image', value: ['string', image] },
    { key: 'url', value: ['string', url] },
    { key: 'fund', value: ['name', fund] },
    { key: 'quantity', value: ['asset', quantity] },
    ...options
  ], { authorization: `${creator}@active` })
}

const updateProp = async (contract, creator, type, title, summary, description, image, url, fund, quantity, options) => {
  await contract.update([
    { key: 'type', value: ['name', type] },
    { key: 'creator', value: ['name', creator] },
    { key: 'title', value: ['string', title] },
    { key: 'summary', value: ['string', summary] },
    { key: 'description', value: ['string', description] },
    { key: 'image', value: ['string', image] },
    { key: 'url', value: ['string', url] },
    { key: 'fund', value: ['name', fund] },
    { key: 'quantity', value: ['asset', quantity] },
    ...options
  ], { authorization: `${creator}@active` })
}

describe('Alliances', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  // const minStake = 1111

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposal')
  await createProp(contracts.dao, firstuser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '10000.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])

  // console.log('changing minimum amount to stake')
  // await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${555}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })

  const propsTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true
  })
  console.log(propsTable.rows)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await contracts.dao.favour(firstuser, 0, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(seconduser, 0, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 0, 10, { authorization: `${thirduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  const escrowTable = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })
  console.log(escrowTable.rows)

})

describe.only('Campaigns', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  const minStake = 1111

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposal')
  await createProp(contracts.dao, firstuser, 'p.camp.inv', 'title', 'summary', 'description', 'image', 'url', campaignbank, '10000.0000 SEEDS', [
    { key: 'max_amount_per_invite', value: ['asset', '10.0000 SEEDS'] },
    { key: 'planted', value: ['asset', '1000.0000 SEEDS'] },
    { key: 'reward', value: ['asset', '5.0000 SEEDS'] },
    { key: 'recipient', value: ['name', firstuser] }
  ])

  const propsTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true
  })
  // console.log("\n", propsTable.rows)

  const propsAuxTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'propaux',
    json: true
  })
  // console.log("\n", JSON.stringify(propsAuxTable.rows, null, 2))

  const escrowTable = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })

  console.log("\n", propsTable.rows)
  console.log("\n", JSON.stringify(propsAuxTable.rows, null, 2))
  console.log('=========================')

  await updateProp(contracts.dao, firstuser, 'p.camp.inv', 'titleU', 'summaryU', 'descriptionU', 'imageU', 'urlU', campaignbank, '2000.0000 SEEDS', [
    { key: 'max_amount_per_invite', value: ['asset', '10.0000 SEEDS'] },
    { key: 'planted', value: ['asset', '2000.0000 SEEDS'] },
    { key: 'reward', value: ['asset', '52.0000 SEEDS'] },
    { key: 'proposal_id', value: ['uint64', 0] },
    { key: 'current_payout', value: ["asset", "20.0000 SEEDS"] },
    { key: 'passed_cycle', value: ['uint64', 2] },
    { key: 'lock_id', value: ['uint64', 0] },
    { key: 'max_age', value: ['uint64', 7] },
    { key: 'recipient', value: ['name', firstuser] }
    // { key: 'max_amount_per_invite', value: ['asset', '10.0000 SEEDS'] },
    // { key: 'planted', value: ['asset', '1000.0000 SEEDS'] },
    // { key: 'reward', value: ['asset', '5.0000 SEEDS'] },
    // { key: 'recipient', value: ['name', firstuser] }
  ])

  const propsTable2 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true
  })

  const propsAuxTable2 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'propaux',
    json: true
  })

  console.log("\n", propsTable2.rows)
  console.log("\n", JSON.stringify(propsAuxTable2.rows, null, 2))

})


