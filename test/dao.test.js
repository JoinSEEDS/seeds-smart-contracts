const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, isLocal, getBalance } = require('../scripts/helper')
const { should, assert } = require('chai')
const { prop, ascend } = require('ramda')

const { 
  dao, token, settings, accounts, harvest, proposals, referendums, firstuser, seconduser, thirduser, fourthuser, fifthuser, scheduler,
  alliancesbank, campaignbank, milestonebank, escrow, onboarding, hyphabank, pool, organization
} = names

const scopes = ['alliance', 'campaign', 'milestone', 'referendum', 'dhos']
const [allianceScope, campaignScope, milestoneScope, referendumsScope, dhosScope] = scopes


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

const checkProp = async (expectedProp, assert, given, should) => {

  const propId = expectedProp.proposal_id

  const propsTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    lower_bound: propId,
    upper_bound: propId
  })

  if (propsTable.rows.length === 0) {
    throw new Error('No proposal found with id:' + propId)
  }

  const prop = propsTable.rows[0]
  let actualProp = {}

  const keys = Object.keys(expectedProp)
  for (const key of keys) {
    actualProp[key] = prop[key]
  }

  const propsAuxTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'propaux',
    json: true,
    lower_bound: propId,
    upper_bound: propId
  })

  if (propsAuxTable.rows.length > 0) {
    const aux = formatSpecialAttributes(propsAuxTable.rows)
    actualProp = {
      ...actualProp,
      ...aux[0]
    }
  }

  console.log(actualProp)

  assert({
    given,
    should,
    actual: actualProp,
    expected: expectedProp
  })

}

const createReferendum = async (contract, creator, settingName, newValue, title, summary, description, image, url) => {
  await createProp(contract, creator, 'ref.setting', title, summary, description, image, url, creator, '0.0000 SEEDS', [
    { key: 'setting_name', value: ['name', settingName] },
    { key: 'new_value', value: newValue },
    { key: 'test_cycles', value: ['uint64', 1] },
    { key: 'eval_cycles', value: ['uint64', 3] }
  ])
}

async function resetContracts () {
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, onboarding, pool })
  console.log('reset dao')
  await contracts.dao.reset({ authorization: `${dao}@active` })
  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })
  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })
  console.log('reset harvest')
  await contracts.harvest.reset({ authorization: `${harvest}@active` })
  console.log('reset onboarding')
  await contracts.onboarding.reset({ authorization: `${onboarding}@active` })
  console.log('reset pool')
  await contracts.pool.reset({ authorization: `${pool}@active` })
  
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

const updateProp = async (contract, creator, type, title, summary, description, image, url, fund, options) => {
  await contract.update([
    { key: 'type', value: ['name', type] },
    { key: 'creator', value: ['name', creator] },
    { key: 'title', value: ['string', title] },
    { key: 'summary', value: ['string', summary] },
    { key: 'description', value: ['string', description] },
    { key: 'image', value: ['string', image] },
    { key: 'url', value: ['string', url] },
    { key: 'fund', value: ['name', fund] },
    ...options
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

  const contracts = await initContracts({ dao, token, settings, accounts, onboarding })

  console.log('reset referendums')
  await contracts.dao.reset({ authorization: `${dao}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

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
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(contracts.dao, seconduser, testSettingFloat, ['float64', testSettingFloatNewValue], 'title 2', 'summary 2', 'description 2', 'image 2', 'url 2')

  console.log('update referendum')
  await contracts.dao.update([
    { key: 'proposal_id', value: ['uint64', 1] },
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
  await contracts.dao.cancel([{ key: 'proposal_id', value: ['uint64', 2] }], { authorization: `${firstuser}@active` })

  const refTables = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true,
    limit: 200
  })

  assert({
    given: 'referendums created',
    should: 'have the correct entries',
    actual: refTables.rows.map(r => {
      delete r.created_at
      return r
    }),
    expected: [
      {
        proposal_id: 1,
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
        type: 'ref.setting',
        last_ran_cycle: 0,
        age: 0,
        fund: firstuser,
        quantity: '0.0000 SEEDS'
      },
      {
        proposal_id: 3,
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
        type: 'ref.setting',
        last_ran_cycle: 0,
        age: 0,
        fund: seconduser,
        quantity: '0.0000 SEEDS'
      }
    ]
  })

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('vote favour for first referendum')
  await voteReferendum(1)

  console.log('staking for second referendum')
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '3', { authorization: `${seconduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkReferendums(
    [
      {
        proposal_id: 1,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'test',
        stage: 'active',
        age: 1,
      },
      {
        proposal_id: 3,
        favour: 0,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'voting',
        stage: 'active',
        age: 0
      }
    ], [
      {
        proposal_id: 1,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 3,
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
  await voteReferendum(3, 3)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkReferendums(
    [
      {
        proposal_id: 1,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 2,
      },
      {
        proposal_id: 3,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'test',
        stage: 'active',
        age: 1
      }
    ], [
      {
        proposal_id: 1,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 3,
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
        proposal_id: 1,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 5,
      },
      {
        proposal_id: 3,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'evaluate',
        stage: 'active',
        age: 4
      }
    ], [
      {
        proposal_id: 1,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 3,
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
        proposal_id: 1,
        favour: 8,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'passed',
        stage: 'done',
        age: 6,
      },
      {
        proposal_id: 3,
        favour: 6,
        against: 0,
        staked: `${minStake}.0000 SEEDS`,
        status: 'passed',
        stage: 'done',
        age: 5
      }
    ], [
      {
        proposal_id: 1,
        is_float: 0,
        new_value: testSettingNewValue,
        previous_value: testSettingPrevVal,
        setting_name: testSetting,
        cycles_per_status: '1,1,4'
      },
      {
        proposal_id: 3,
        is_float: 1,
        new_value: testSettingFloatNewValue,
        previous_value: testSettingFloatPrevVal,
        setting_name: testSettingFloat,
        cycles_per_status: '1,1,3'
      }
    ]
  )

  console.log('===========================================================')
  console.log('failing referendums')

  console.log('create referendum')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 5], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(contracts.dao, seconduser, testSettingFloat, ['float64', 0.2], 'title 2', 'summary 2', 'description 2', 'image 2', 'url 2')
  await createReferendum(contracts.dao, thirduser, testSettingFloat, ['float64', 22.2], 'title 3', 'summary 3', 'description 3', 'image 3', 'url 3')
  await createReferendum(contracts.dao, thirduser, testSettingFloat, ['float64', 3.2], 'title 4', 'summary 4', 'description 4', 'image 4', 'url 4')

  console.log('staking for referendums')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '4', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '5', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, dao, `${minStake}.0000 SEEDS`, '6', { authorization: `${thirduser}@active` })
  await contracts.token.transfer(thirduser, dao, `${minStake}.0000 SEEDS`, '7', { authorization: `${thirduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await voteReferendum(4)
  await voteReferendum(5)
  await voteReferendum(6, 4, false)
  await voteReferendum(7)

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('change trust for referendum 1')
  await revertVote(5, 3)

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
  await revertVote(4, 3)
  await revertVote(7, 3)

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
    actual: refTables2.rows.filter(r => r.proposal_id > 3).map(r => {
      return {
        proposal_id: r.proposal_id,
        status: r.status,
        stage: r.stage
      }
    }),
    expected: Array.from(Array(4).keys()).map(key => {
      return {
        proposal_id: key + 4,
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

  const checkRep = async (expected) => {
    const repTable = await getTableRows({
      code: accounts,
      scope: accounts,
      table: 'rep',
      json: true
    })
    const actual = repTable.rows.map(r => {
      return {
        account: r.account,
        rep: r.rep
      }
    })
    assert({
      given: 'users voted',
      should: 'have the correct reputation',
      actual,
      expected
    })
  }

  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', 'high', { authorization: `${settings}@active` })

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposals')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createProp(contracts.dao, seconduser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '10000.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '2', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${555}.0000 SEEDS`, '3', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('voting for proposals')
  await contracts.dao.favour(firstuser, 1, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(firstuser, 2, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(firstuser, 3, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.against(seconduser, 1, 5, { authorization: `${seconduser}@active` })
  await contracts.dao.neutral(thirduser, 1, { authorization: `${thirduser}@active` })
  await contracts.dao.favour(thirduser, 2, 10, { authorization: `${thirduser}@active` })
  await contracts.dao.against(thirduser, 3, 10, { authorization: `${thirduser}@active` })

  await checkParticipants([
    { account: firstuser, nonneutral: 1, count: 3 },
    { account: seconduser, nonneutral: 1, count: 1 },
    { account: thirduser, nonneutral: 0, count: 3 }
  ])

  await checkRep([
    { account: firstuser, rep: 1 },
    { account: seconduser, rep: 1 },
    { account: thirduser, rep: 1 }
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

  await checkRep([
    { account: firstuser, rep: 6 },
    { account: seconduser, rep: 1 },
    { account: thirduser, rep: 1 }
  ])

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

  console.log('create proposals')
  await createReferendum(contracts.dao, firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createProp(contracts.dao, firstuser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])
  await createProp(contracts.dao, seconduser, 'p.milestone', 'title', 'summary', 'description', 'image', 'url', milestonebank, '55.7000 SEEDS', [
    { key: 'recipient', value: ['name', hyphabank] }
  ])

  console.log('changing minimum amount to stake')
  await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '2', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '', { authorization: `${seconduser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('voting for proposal with scope referendums')
  await contracts.dao.favour(firstuser, 1, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.against(seconduser, 1, 5, { authorization: `${seconduser}@active` })
  await contracts.dao.neutral(thirduser, 1, { authorization: `${thirduser}@active` })
  await contracts.dao.favour(firstuser, 2, 5, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(firstuser, 3, 1, { authorization: `${firstuser}@active` })

  const actualVoices = []
  
  for (const user of users) {
    const voices = await getVoice(user)
    actualVoices.push(voices)
  }

  assert({
    given: 'vote done for a referendum',
    should: 'have the correct scope',
    actual: actualVoices,
    expected: [
      [
        { scope: allianceScope, account: firstuser, balance: 99 },
        { scope: campaignScope, account: firstuser, balance: 94 },
        { scope: milestoneScope, account: firstuser, balance: 98 },
        { scope: referendumsScope, account: firstuser, balance: 89 },
        { scope: 'dhos', account: firstuser, balance: 99 }
      ],
      [
        { scope: allianceScope, account: seconduser, balance: 99 },
        { scope: campaignScope, account: seconduser, balance: 99 }, 
        { scope: milestoneScope, account: seconduser, balance: 99 },
        { scope: referendumsScope, account: seconduser, balance: 94 },
        { scope: 'dhos', account: seconduser, balance: 99 }
      ],
      [
        { scope: allianceScope, account: thirduser, balance: 99 },
        { scope: campaignScope, account: thirduser, balance: 99 }, 
        { scope: milestoneScope, account: thirduser, balance: 99 },
        { scope: referendumsScope, account: thirduser, balance: 99 },
        { scope: 'dhos', account: thirduser, balance: 99 }
      ],
      [
        { scope: allianceScope, account: fourthuser, balance: 99 },
        { scope: campaignScope, account: fourthuser, balance: 99 }, 
        { scope: milestoneScope, account: fourthuser, balance: 99 },
        { scope: referendumsScope, account: fourthuser, balance: 99 },
        { scope: 'dhos', account: fourthuser, balance: 99 }
      ]
    ]
  })

  const votesTable = await getTableRows({
    code: dao,
    scope: 1,
    table: 'votes',
    json: true
  })

  assert({
    given: 'vote done for a referendum',
    should: 'have the votes',
    actual: votesTable.rows,
    expected: [
      { proposal_id: 1, account: firstuser, amount: 10, favour: 1 },
      { proposal_id: 1, account: seconduser, amount: 5, favour: 0 },
      { proposal_id: 1, account: thirduser, amount: 0, favour: 0 }
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
    expected: [1,2,3]
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
      actual: actualVoices.map(av => av.filter(a => a.scope === referendumsScope)[0]),
      expected: expectedVoices
    })

  }

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
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('delegate voice')
  await contracts.dao.delegate(seconduser, firstuser, referendumsScope, { authorization: `${seconduser}@active` })
  await contracts.dao.delegate(thirduser, firstuser, referendumsScope, { authorization: `${thirduser}@active` })
  await contracts.dao.delegate(fourthuser, thirduser, referendumsScope, { authorization: `${fourthuser}@active` })

  console.log('voting')
  await contracts.dao.favour(firstuser, 1, 5, { authorization: `${firstuser}@active` })
  await sleep(5000)

  await checkVoices([
    { scope: referendumsScope, account: firstuser, balance: 15 },
    { scope: referendumsScope, account: seconduser, balance: 30 },
    { scope: referendumsScope, account: thirduser, balance: 45 },
    { scope: referendumsScope, account: fourthuser, balance: 60 }
  ])

  console.log('Undelegate voice')
  await contracts.dao.undelegate(seconduser, referendumsScope, { authorization: `${seconduser}@active` })

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

  await contracts.dao.revertvote(firstuser, 1, { authorization: `${firstuser}@active` })
  await sleep(5000)

  await checkVoices([
    { scope: referendumsScope, account: firstuser, balance: 15 },
    { scope: referendumsScope, account: seconduser, balance: 30 },
    { scope: referendumsScope, account: thirduser, balance: 45 },
    { scope: referendumsScope, account: fourthuser, balance: 60 }
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

describe('Stake burn in rejected proposals', async assert => {
  
  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  const printProps = async () => {
    const proposalsTable = await eos.getTableRows({
      code: dao,
      scope: dao,
      table: 'proposals',
      json: true,
    })

    console.log(proposalsTable)
  }

  const testStake = async (expectedValues) => {
    const proposalsTable = await eos.getTableRows({
      code: dao,
      scope: dao,
      table: 'proposals',
      json: true,
    })
    assert({
      given: 'onperiod ran',
      should: 'have correct stake amount',
      actual: proposalsTable.rows.map(r => r.staked),
      expected: expectedValues
    })    
  }

  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('reset escrow')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  const minStake =  100

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('set settings')
  await contracts.settings.configure("prop.cmp.min", 2 * 10000, { authorization: `${settings}@active` })
  await contracts.settings.configure("prop.al.min", 2 * 10000, { authorization: `${settings}@active` })
  
  await contracts.settings.configure("propquorum", 50, { authorization: `${settings}@active` }) 
  await contracts.settings.configure("propmajority", 60, { authorization: `${settings}@active` })

  console.log('create proposal')

  await createProp(contracts.dao, firstuser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])

  await createProp(contracts.dao, firstuser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])

  await createProp(contracts.dao, seconduser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', seconduser] }
  ])

  await createProp(contracts.dao, seconduser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', seconduser] }
  ])

  await testStake(['0.0000 SEEDS', '0.0000 SEEDS', '0.0000 SEEDS', '0.0000 SEEDS' ])

  const firstBalances = [
    await getBalance(firstuser),
    await getBalance(seconduser),
    await getBalance(campaignbank),
    await getBalance(alliancesbank)
  ]

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${minStake}.0000 SEEDS`, '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '3', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(seconduser, dao, `${minStake}.0000 SEEDS`, '', { authorization: `${seconduser}@active` })

  await testStake(['100.0000 SEEDS', '100.0000 SEEDS', '100.0000 SEEDS', '100.0000 SEEDS' ])

  const stakeBalances = [
    await getBalance(firstuser),
    await getBalance(seconduser),
    await getBalance(campaignbank),
    await getBalance(alliancesbank)
  ]
 
  console.log('move proposals to active')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(3000)

  console.log('vote on first proposal') // unity but not quorum (loses 5% of stake)
  await contracts.dao.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })

  console.log('vote on second proposal') // not unity but quorum (loses 100% of stake)
  await contracts.dao.favour(seconduser, 2, 20, { authorization: `${seconduser}@active` })
  await contracts.dao.against(firstuser, 2, 30, { authorization: `${firstuser}@active` })

  console.log('vote on 4th proposal') // not quorum either unity (loses 100% of stake)
  await contracts.dao.against(seconduser, 4, 11, { authorization: `${seconduser}@active` })

  // await printProps()

  console.log('execute proposals')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(3000)

  const props = await getTableRows({
    code: dao,
    scope: dao,
    table: 'proposals',
    json: true
  })

  await testStake(['0.0000 SEEDS', '0.0000 SEEDS', '0.0000 SEEDS', '0.0000 SEEDS' ])

  const finalBalances = [
    await getBalance(firstuser),
    await getBalance(seconduser),
    await getBalance(campaignbank),
    await getBalance(alliancesbank)
  ]

  assert({
    given: 'firstuser transfer to proposals',
    should: 'have correct balance',
    actual: firstBalances[0] - stakeBalances[0],
    expected: 200
  })

  assert({
    given: 'seconduser transfer to proposals',
    should: 'have correct balance',
    actual: firstBalances[1] - stakeBalances[1],
    expected: 200
  })

  assert({
    given: 'failed proposal quorum',
    should: 'be rejected',
    actual: props.rows[0].status,
    expected: 'rejected'
  })

  assert({
    given: 'failed proposal quorum majority',
    should: 'be rejected',
    actual: props.rows[1].status,
    expected: "rejected"
  })

  assert({
    given: 'unity met with invalid quorum',
    should: 'return 0.95 of stake',
    actual: finalBalances[0] - stakeBalances[0],
    expected: 95
  })

  assert({
    given: 'unity not met',
    should: 'burn stake',
    actual: finalBalances[1] - stakeBalances[1],
    expected: 0
  })

  // await printProps()

})
  
describe('Voice Erase', async assert => {
  
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
  await contracts.settings.configure('batchsize', 2, { authorization: `${settings}@active` })

  await contracts.dao.updatevoices({ authorization: `${dao}@active` })
  await sleep(2000)
  
  await contracts.dao.deletescope(0, allianceScope, { authorization: `${dao}@active` })
  await sleep(2000)

  const actualVoices = []
  
  for (const user of users) {
    const voices = await getVoice(user)
    actualVoices.push(voices)
  }

  assert({
    given: 'erase voices from alliance',
    should: 'have no points for alliance',
    actual: actualVoices,
    expected: [
      [
        { scope: campaignScope, account: firstuser, balance: 20 },
        { scope: milestoneScope, account: firstuser, balance: 20 },
        { scope: referendumsScope, account: firstuser, balance: 20 },
        { scope: dhosScope, account: firstuser, balance: 20}
      ],
      [
        { scope: campaignScope, account: seconduser, balance: 40 }, 
        { scope: milestoneScope, account: seconduser, balance: 40 },
        { scope: referendumsScope, account: seconduser, balance: 40 },
        { scope: dhosScope, account: seconduser, balance: 40}
      ],
      [
        { scope: campaignScope, account: thirduser, balance: 60 }, 
        { scope: milestoneScope, account: thirduser, balance: 60 },
        { scope: referendumsScope, account: thirduser, balance: 60 },
        { scope: dhosScope, account: thirduser, balance: 60}
      ],
      [
        { scope: campaignScope, account: fourthuser, balance: 80 }, 
        { scope: milestoneScope, account: fourthuser, balance: 80 },
        { scope: referendumsScope, account: fourthuser, balance: 80 },
        { scope: dhosScope, account: fourthuser, balance: 80}
      ]
    ]
  })

  console.log('Add alliance voices');

  await contracts.dao.addvoice(0, allianceScope, { authorization: `${dao}@active` })
  await sleep(2000)

  const newVoices = []
  
  for (const user of users) {
    const voices = await getVoice(user)
    newVoices.push(voices)
  }

  assert({
    given: 'add voice alliance',
    should: 'add alliance scope',
    actual: newVoices,
    expected: [
      [
        { scope: allianceScope, account: firstuser, balance: 20},
        { scope: campaignScope, account: firstuser, balance: 20 },
        { scope: milestoneScope, account: firstuser, balance: 20 },
        { scope: referendumsScope, account: firstuser, balance: 20 },
        { scope: dhosScope, account: firstuser, balance: 20}
      ],
      [
        { scope: allianceScope, account: seconduser, balance: 40},
        { scope: campaignScope, account: seconduser, balance: 40 }, 
        { scope: milestoneScope, account: seconduser, balance: 40 },
        { scope: referendumsScope, account: seconduser, balance: 40 },
        { scope: dhosScope, account: seconduser, balance: 40}
      ],
      [
        { scope: allianceScope, account: thirduser, balance: 60},
        { scope: campaignScope, account: thirduser, balance: 60 }, 
        { scope: milestoneScope, account: thirduser, balance: 60 },
        { scope: referendumsScope, account: thirduser, balance: 60 },
        { scope: dhosScope, account: thirduser, balance: 60}
      ],
      [
        { scope: allianceScope, account: fourthuser, balance: 80},
        { scope: campaignScope, account: fourthuser, balance: 80 }, 
        { scope: milestoneScope, account: fourthuser, balance: 80 },
        { scope: referendumsScope, account: fourthuser, balance: 80 },
        { scope: dhosScope, account: fourthuser, balance: 80}
      ]
    ]

  })

})

describe('Alliance Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()
  
  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('reset escrow')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  const minStake = 1111

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposal')
  await createProp(contracts.dao, firstuser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '10000.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])
  await createProp(contracts.dao, firstuser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '1000000.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])
  await createProp(contracts.dao, seconduser, 'p.alliance', 'title', 'summary', 'description', 'image', 'url', alliancesbank, '1000000.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])

  // console.log('changing minimum amount to stake')
  // await contracts.settings.configure('refsnewprice', minStake * 10000, { authorization: `${settings}@active` })

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${555}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${10000}.0000 SEEDS`, '', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(firstuser, dao, `${10000}.0000 SEEDS`, '3', { authorization: `${firstuser}@active` })

  await contracts.dao.update([
    { key: 'proposal_id', value: ['uint64', 2] },
    { key: 'title', value: ['string', 'title updated'] },
    { key: 'summary', value: ['string', 'summary updated'] },
    { key: 'description', value: ['string', 'description updated'] },
    { key: 'image', value: ['string', 'image updated'] },
    { key: 'url', value: ['string', 'url updated'] }
  ], { authorization: `${firstuser}@active` })

  await checkProp(
    {
      proposal_id: 1,
      favour: 0,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.alliance',
      last_ran_cycle: 0,
      age: 0,
      fund: alliancesbank,
      quantity: '10000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      lock_id: 0,
      passed_cycle: 0,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'prop alliance created',
    'create an entry in props table'
  )

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await contracts.dao.favour(firstuser, 1, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 1, 10, { authorization: `${thirduser}@active` })

  await contracts.dao.favour(firstuser, 2, 9, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(seconduser, 2, 9, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 2, 9, { authorization: `${thirduser}@active` })

  await contracts.dao.against(firstuser, 3, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(seconduser, 3, 9, { authorization: `${seconduser}@active` })
  await contracts.dao.neutral(thirduser, 3, { authorization: `${thirduser}@active` })

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '555.0000 SEEDS',
      status: 'open',
      stage: 'active',
      quantity: '10000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      lock_id: 0,
      passed_cycle: 0,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'on period ran',
    'move prop to active'
  )
  await checkProp(
    {
      proposal_id: 2,
      favour: 27,
      against: 0,
      staked: '10000.0000 SEEDS',
      status: 'open',
      stage: 'active',
      quantity: '1000000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      lock_id: 0,
      passed_cycle: 0,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'on period ran',
    'move prop to active'
  )

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 3,
      favour: 9,
      against: 10,
      staked: '0.0000 SEEDS',
      creator: seconduser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      stage: 'done',
      type: 'p.alliance',
      last_ran_cycle: 2,
      age: 0,
      fund: 'allies.seeds',
      quantity: '1000000.0000 SEEDS',
      current_payout: "0.0000 SEEDS",
      lock_id: 0,
      passed_cycle: 2,
      recipient: "seedsuseraaa",
      executed: 0
    },
    assert,
    'on period ran',
    'reject proposal'
  )

  const escrowTable = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })

  assert({
    given: 'proposals accepted',
    should: 'create locks',
    actual: escrowTable.rows.map(r => {
      delete r.vesting_date
      delete r.created_date
      delete r.updated_date
      return r
    }),
    expected:   [
      {
        id: 0,
        lock_type: 'event',
        sponsor: 'allies.seeds',
        beneficiary: firstuser,
        quantity: '10000.0000 SEEDS',
        trigger_event: 'golive',
        trigger_source: 'dao.hypha',
        notes: 'proposal_id: 1'
      },
      {
        id: 1,
        lock_type: 'event',
        sponsor: 'allies.seeds',
        beneficiary: firstuser,
        quantity: '1000000.0000 SEEDS',
        trigger_event: 'golive',
        trigger_source: 'dao.hypha',
        notes: 'proposal_id: 2'
      }
    ]
  })

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '0.0000 SEEDS',
      status: 'evaluate',
      stage: 'active',
      quantity: '10000.0000 SEEDS',
      current_payout: '10000.0000 SEEDS',
      lock_id: 0,
      passed_cycle: 2,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'on period ran',
    'move prop to evaluate'
  )
  await checkProp(
    {
      proposal_id: 2,
      favour: 27,
      against: 0,
      staked: '0.0000 SEEDS',
      status: 'evaluate',
      stage: 'active',
      quantity: '1000000.0000 SEEDS',
      current_payout: '1000000.0000 SEEDS',
      lock_id: 1,
      passed_cycle: 2,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'on period ran',
    'move prop to evaluate'
  )

  console.log('running onperiod more times...')
  for (let i = 0; i < 2; i++) {
    await contracts.dao.onperiod({ authorization: `${dao}@active` })
    await sleep(2000)
  }

  console.log('voting down for prop 2')
  await contracts.dao.revertvote(firstuser, 2, { authorization: `${firstuser}@active` })
  await contracts.dao.revertvote(seconduser, 2, { authorization: `${seconduser}@active` })


  console.log('triggering go live event')
  const eventSource = 'dao.hypha'
  const eventName = 'golive'

  console.log('going live')
  await contracts.escrow.resettrigger(eventSource, { authorization: `${escrow}@active` })
  await contracts.escrow.triggertest(eventSource, eventName, 'test event', { authorization: `${escrow}@active` })
  await sleep(5000)

  const poolBalances = await eos.getTableRows({
    code: pool,
    scope: pool,
    table: 'balances',
    json: true
  })
  
  assert({
    given: 'go live triggered',
    should: 'have the correct pool balances',
    actual: poolBalances.rows,
    expected: [
      { account: firstuser, balance: '10000.0000 SEEDS' }
    ]
  })

  const escrowTableBefore = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })

  assert({
    given: 'go live triggered',
    should: 'not mig the lock for the failing proposal',
    actual: escrowTableBefore.rows.length,
    expected: 1
  })

  console.log('running on period')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '0.0000 SEEDS',
      status: 'passed',
      stage: 'done',
      quantity: '10000.0000 SEEDS',
      last_ran_cycle: 4,
      current_payout: '10000.0000 SEEDS',
      lock_id: 0,
      passed_cycle: 2,
      recipient: firstuser,
      executed: 1
    },
    assert,
    'on period ran',
    'keep prop on evaluate'
  )
  await checkProp(
    {
      proposal_id: 2,
      favour: 9,
      against: 18,
      staked: '0.0000 SEEDS',
      status: 'rejected',
      stage: 'done',
      quantity: '1000000.0000 SEEDS',
      last_ran_cycle: 5,
      current_payout: '1000000.0000 SEEDS',
      lock_id: 1,
      passed_cycle: 2,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'on period ran',
    'move prop to reject state'
  )

  const escrowTableAfter = await getTableRows({
    code: escrow,
    scope: escrow,
    table: 'locks',
    json: true
  })

  assert({
    given: 'proposal cancel',
    should: 'cancel lock',
    actual: escrowTableAfter.rows,
    expected: []
  })

})

describe('Invite Campaign Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create prop')
  await createProp(contracts.dao, firstuser, 'p.camp.inv', 'title', 'summary', 'description', 'image', 'url', campaignbank, '1000.0000 SEEDS', [
    { key: 'max_amount_per_invite', value: ['asset', '20.0000 SEEDS'] },
    { key: 'planted', value: ['asset', '6.0000 SEEDS'] },
    { key: 'reward', value: ['asset', '2.0000 SEEDS'] },
    { key: 'reward_owner', value: ['name', seconduser] }
  ])

  await checkProp(
    {
      proposal_id: 1,
      favour: 0,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.camp.inv',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '1000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      executed: 0
    },
    assert,
    'prop campaign invite created',
    'create entry in props table'
  )

  console.log('staking')
  await contracts.token.transfer(firstuser, dao, `${555}.0000 SEEDS`, '1', { authorization: `${firstuser}@active` })

  console.log('update proposal')
  await updateProp(contracts.dao, firstuser, 'p.camp.inv', 'titleU', 'summaryU', 'descriptionU', 'imageU', 'urlU', campaignbank, [
    { key: 'proposal_id', value: ['uint64', 1] }
  ])

  await checkProp(
    {
      proposal_id: 1,
      favour: 0,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: firstuser,
      title: 'titleU',
      summary: 'summaryU',
      description: 'descriptionU',
      image: 'imageU',
      url: 'urlU',
      status: 'open',
      stage: 'staged',
      type: 'p.camp.inv',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '1000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      executed: 0
    },
    assert,
    'prop campaign invite updated',
    'update entry in props table'
  )

  console.log('running onperiod')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await contracts.dao.favour(firstuser, 1, 10, { authorization: `${firstuser}@active` })
  await contracts.dao.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 1, 10, { authorization: `${thirduser}@active` })

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: firstuser,
      title: 'titleU',
      summary: 'summaryU',
      description: 'descriptionU',
      image: 'imageU',
      url: 'urlU',
      status: 'open',
      stage: 'active',
      type: 'p.camp.inv',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '1000.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      executed: 0
    },
    assert,
    'prop campaign votes',
    'update campaign votes'
  )

  console.log('running onperiod 2')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'titleU',
      summary: 'summaryU',
      description: 'descriptionU',
      image: 'imageU',
      url: 'urlU',
      status: 'evaluate',
      stage: 'active',
      type: 'p.camp.inv',
      last_ran_cycle: 2,
      age: 0,
      fund: campaignbank,
      quantity: '1000.0000 SEEDS',
      current_payout: '1000.0000 SEEDS',
      passed_cycle: 2,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      campaign_id: 1,
      executed: 1
    },
    assert,
    'prop campaign on period, stage active, status open',
    'update status to evaluate, move staked to creator'
  )

  for (let i = 0; i < 6; i++) {
    await contracts.dao.onperiod({ authorization: `${dao}@active` })
    await sleep(2000)
  }

  await checkProp(
    {
      proposal_id: 1,
      favour: 30,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'titleU',
      summary: 'summaryU',
      description: 'descriptionU',
      image: 'imageU',
      url: 'urlU',
      status: 'passed',
      stage: 'done',
      type: 'p.camp.inv',
      last_ran_cycle: 8,
      age: 6,
      fund: campaignbank,
      quantity: '1000.0000 SEEDS',
      current_payout: '1000.0000 SEEDS',
      passed_cycle: 2,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      campaign_id: 1,
      executed: 1
    },
    assert,
    'prop campaign votes',
    'update campaign votes'
  )

  console.log('Use case - Create proposal but it will not voted so it will be rejected')

  await createProp(contracts.dao, firstuser, 'p.camp.inv', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'max_amount_per_invite', value: ['asset', '20.0000 SEEDS'] },
    { key: 'planted', value: ['asset', '6.0000 SEEDS'] },
    { key: 'reward', value: ['asset', '2.0000 SEEDS'] },
    { key: 'reward_owner', value: ['name', seconduser] }
  ])

  console.log('staking')
  await contracts.token.transfer(seconduser, dao, `${555}.0000 SEEDS`, '2', { authorization: `${seconduser}@active` })

  for (let i = 0; i < 6; i++) {
    await contracts.dao.onperiod({ authorization: `${dao}@active` })
    await sleep(2000)
  }

  await checkProp(
    {
      proposal_id: 2,
      favour: 0,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      stage: 'done',
      type: 'p.camp.inv',
      last_ran_cycle: 10,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 10,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      executed: 0
    },
    assert,
    'prop campaign invite created',
    'create entry in props table'
  )

  console.log('Use case - Create proposal then its voted and reverted')

  await createProp(contracts.dao, firstuser, 'p.camp.inv', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'max_amount_per_invite', value: ['asset', '20.0000 SEEDS'] },
    { key: 'planted', value: ['asset', '6.0000 SEEDS'] },
    { key: 'reward', value: ['asset', '2.0000 SEEDS'] },
    { key: 'reward_owner', value: ['name', seconduser] }
  ])

  console.log('staking')
  await contracts.token.transfer(seconduser, dao, `${555}.0000 SEEDS`, '3', { authorization: `${seconduser}@active` })

  console.log('on period')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('favour voting')
  await contracts.dao.favour(thirduser, 3, 10, { authorization: `${thirduser}@active` })
  await contracts.dao.favour(seconduser, 3, 10, { authorization: `${seconduser}@active` })

  await checkProp(
    {
      proposal_id: 3,
      favour: 20,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'active',
      type: 'p.camp.inv',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      executed: 0
    },
    assert,
    'prop campaign invite created',
    'create entry in props table'
  )

  console.log('on period')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 3,
      favour: 20,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'evaluate',
      stage: 'active',
      type: 'p.camp.inv',
      last_ran_cycle: 16,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '100.0000 SEEDS',
      passed_cycle: 16,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      campaign_id: 2,
      executed: 1
    },
    assert,
    'prop campaign invite created',
    'create entry in props table'
  )

  console.log('rever votes');
  await contracts.dao.revertvote(thirduser, 3, { authorization: `${thirduser}@active` })
  await contracts.dao.revertvote(seconduser, 3, { authorization: `${seconduser}@active` })

  const campaignsTableBefore = await getTableRows({
    code: onboarding,
    scope: onboarding,
    table: 'campaigns',
    json: true
  })

  assert({
    given: 'After proposal passed',
    should: 'create entry in campaings table',
    actual: campaignsTableBefore.rows[1],
    expected: {
      campaign_id: 2,
      type: "invite",
      origin_account: dao,
      owner: firstuser,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward_owner: seconduser,
      reward: "2.0000 SEEDS",
      authorized_accounts: [firstuser],
      total_amount: "100.0000 SEEDS",
      remaining_amount: "100.0000 SEEDS"
    }
  })

  console.log('on period')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 3,
      favour: 0,
      against: 20,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'rejected',
      stage: 'done',
      type: 'p.camp.inv',
      last_ran_cycle: 17,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '100.0000 SEEDS',
      passed_cycle: 16,
      reward_owner: seconduser,
      max_age: 6,
      max_amount_per_invite: "20.0000 SEEDS",
      planted: "6.0000 SEEDS",
      reward: '2.0000 SEEDS',
      campaign_id: 2,
      executed: 1
    },
    assert,
    'prop campaign invite created',
    'create entry in props table'
  )

  const campaignsTableAfter = await getTableRows({
    code: onboarding,
    scope: onboarding,
    table: 'campaigns',
    json: true
  })

  assert({
    given: 'After proposal rejected on evaluate status',
    should: 'delete entry in campaings table',
    actual: campaignsTableAfter.rows[1],
    expected: undefined
  })
})



describe('Milestone Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposals')
  await createProp(contracts.dao, firstuser, 'p.milestone', 'title', 'summary', 'description', 'image', 'url', milestonebank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', hyphabank] }
  ])
  await createProp(contracts.dao, seconduser, 'p.milestone', 'title', 'summary', 'description', 'image', 'url', milestonebank, '55.7000 SEEDS', [
    { key: 'recipient', value: ['name', hyphabank] }
  ])

  console.log('deposit stake (memo 1)')
  await contracts.token.transfer(firstuser, dao, '555.0000 SEEDS', '1', { authorization: `${firstuser}@active` })

  console.log('deposit stake (memo 2)')
  await contracts.token.transfer(seconduser, dao, '555.0000 SEEDS', '2', { authorization: `${seconduser}@active` })

  const hyphaBalanceBefore = await getBalance(hyphabank)

  await checkProp(
    {
      proposal_id: 1,
      favour: 0,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.milestone',
      last_ran_cycle: 0,
      age: 0,
      fund: milestonebank,
      quantity: '100.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      recipient: hyphabank,
      executed: 0
    },
    assert,
    'prop created',
    'have an entry in the props table'
  )
  await checkProp(
    {
      proposal_id: 2,
      favour: 0,
      against: 0,
      staked: '555.0000 SEEDS',
      creator: seconduser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.milestone',
      last_ran_cycle: 0,
      age: 0,
      fund: milestonebank,
      quantity: '55.7000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 0,
      recipient: hyphabank,
      executed: 0
    },
    assert,
    'prop created',
    'have an entry in the props table'
  )

  console.log('move proposals to active')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('approve proposal 1')
  await contracts.dao.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 1, 10, { authorization: `${thirduser}@active` })

  console.log('running on period')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  await checkProp(
    {
      proposal_id: 1,
      favour: 20,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'passed',
      stage: 'done',
      type: 'p.milestone',
      last_ran_cycle: 2,
      age: 0,
      fund: milestonebank,
      quantity: '100.0000 SEEDS',
      current_payout: '100.0000 SEEDS',
      passed_cycle: 2,
      recipient: hyphabank,
      executed: 1
    },
    assert,
    'proposal approved',
    'receive the funds'
  )

  await checkProp(
    {
      proposal_id: 2,
      favour: 0,
      against: 0,
      staked: '0.0000 SEEDS',
      status: 'rejected',
      stage: 'done',
      type: 'p.milestone',
      last_ran_cycle: 2,
      age: 0,
      fund: milestonebank,
      quantity: '55.7000 SEEDS',
      current_payout: '0.0000 SEEDS',
      passed_cycle: 2,
      recipient: hyphabank,
      executed: 0
    },
    assert,
    'proposal approved',
    'receive the funds'
  )

  const hyphaBalanceAfter = await getBalance(hyphabank)

  assert({
    given: 'milestone approved',
    should: `send the funds to ${hyphabank}`,
    actual: hyphaBalanceAfter - hyphaBalanceBefore,
    expected: 100
  })

})

describe('Funding Campaign Proposals', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow })
  await Promise.all(users.map(user => contracts.dao.testsetvoice(user, 99, { authorization: `${dao}@active` })))

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create proposals')
  await createProp(contracts.dao, firstuser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', firstuser] }
  ])
  await createProp(contracts.dao, seconduser, 'p.camp.fnd', 'title', 'summary', 'description', 'image', 'url', campaignbank, '100.0000 SEEDS', [
    { key: 'recipient', value: ['name', seconduser] },
    { key: 'pay_percentages', value: ['string', '15,50,25,10'] }
  ])

  await checkProp(
    {
      proposal_id: 1,
      favour: 0,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: firstuser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.camp.fnd',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      pay_percentages: '25,25,25,25',
      passed_cycle: 0,
      recipient: firstuser,
      executed: 0
    },
    assert,
    'proposal created',
    'have the correct values'
  )
  await checkProp(
    {
      proposal_id: 2,
      favour: 0,
      against: 0,
      staked: '0.0000 SEEDS',
      creator: seconduser,
      title: 'title',
      summary: 'summary',
      description: 'description',
      image: 'image',
      url: 'url',
      status: 'open',
      stage: 'staged',
      type: 'p.camp.fnd',
      last_ran_cycle: 0,
      age: 0,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '0.0000 SEEDS',
      pay_percentages: '15,50,25,10',
      passed_cycle: 0,
      recipient: seconduser,
      executed: 0
    },
    assert,
    'proposal created',
    'have the correct values'
  )

  console.log('deposit stake')
  await contracts.token.transfer(firstuser, dao, '555.0000 SEEDS', '1', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, dao, '555.0000 SEEDS', '2', { authorization: `${seconduser}@active` })

  console.log('move proposals to active')
  await contracts.dao.onperiod({ authorization: `${dao}@active` })
  await sleep(2000)

  console.log('approve proposal 1')
  await contracts.dao.favour(seconduser, 1, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 1, 10, { authorization: `${thirduser}@active` })

  await contracts.dao.favour(seconduser, 2, 10, { authorization: `${seconduser}@active` })
  await contracts.dao.favour(thirduser, 2, 10, { authorization: `${thirduser}@active` })

  const checkPayout = async (expected) => {

    const balanceBefore1 = await getBalance(firstuser)
    const balanceBefore2 = await getBalance(seconduser)

    console.log('running on period')
    await contracts.dao.onperiod({ authorization: `${dao}@active` })
    await sleep(2000)
  
    const balanceAfter1 = await getBalance(firstuser)
    const balanceAfter2 = await getBalance(seconduser)
  
    assert({
      given: 'onperiod ran',
      should: 'payout the first percentage',
      actual: [
        balanceAfter1 - balanceBefore1,
        balanceAfter2 - balanceBefore2
      ],
      expected
    })

  }
  await checkPayout([25 + 555, 15 + 555])
  await checkPayout([25, 50])

  console.log('voting down proposal 2')
  await contracts.dao.revertvote(thirduser, 2, { authorization: `${thirduser}@active` })
  await contracts.dao.revertvote(seconduser, 2, { authorization: `${seconduser}@active` })

  await checkPayout([25, 0])
  await checkPayout([25, 0])
  await checkPayout([0, 0])

  await checkProp(
    {
      proposal_id: 1,
      status: 'passed',
      stage: 'done',
      type: 'p.camp.fnd',
      last_ran_cycle: 5,
      age: 3,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '100.0000 SEEDS',
      pay_percentages: '25,25,25,25',
      passed_cycle: 2,
      recipient: firstuser,
      executed: 1
    },
    assert,
    'proposal ran',
    'payout the correct values'
  )

  await checkProp(
    {
      proposal_id: 2,
      status: 'rejected',
      stage: 'done',
      type: 'p.camp.fnd',
      last_ran_cycle: 4,
      age: 1,
      fund: campaignbank,
      quantity: '100.0000 SEEDS',
      current_payout: '65.0000 SEEDS',
      pay_percentages: '15,50,25,10',
      passed_cycle: 2,
      recipient: seconduser,
      executed: 0
    },
    assert,
    'proposal ran',
    'payout the correct values'
  )

})


describe('Streaming Funding', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()

  const users = [firstuser, seconduser, thirduser, fourthuser]
  const contracts = await initContracts({ dao, token, settings, accounts, harvest, escrow, organization })
  await Promise.all(users.map((user, index) => contracts.harvest.testupdatecs(user, 10 * index, { authorization: `${harvest}@active` })))
  await Promise.all(users.map(user => contracts.dao.changetrust(user, true, { authorization: `${dao}@active` })))

  const csTable = await getTableRows({
    code: harvest,
    scope: harvest,
    table: 'cspoints',
    json: true
  })

  console.log(csTable)

  console.log('reset orgs')
  await contracts.organization.reset({ authorization: `${organization}@active` })

  console.log('change batch size to 1')
  await contracts.settings.configure('batchsize', 1, { authorization: `${settings}@active` })

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  console.log('create organizations')
  const eosDevKey = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'
  
  let orgs = ['org1', 'org2', 'org3', 'org4', 'org5', 'orgt']
  
  await contracts.token.transfer(firstuser, organization, `${200 * orgs.length}.0000 SEEDS`, '', { authorization: `${firstuser}@active` })
  await Promise.all(orgs.map(org => contracts.organization.create(firstuser, org, org, eosDevKey, { authorization: `${firstuser}@active` })))


  const checkDhos = async ({ expected, should, given }) => {

    const dhosTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'dhos',
      json: true
    })
    
    const totalSize = expected.map(e => e.points).reduce((acc, curr) => acc + curr)

    const sizesTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'sizes',
      json: true
    })

    assert({
      given,
      should,
      expected,
      actual: dhosTable.rows
    })

    assert({
      given: 'users voted for dhos',
      should: 'have the correct size',
      expected: totalSize,
      actual: (sizesTable.rows.filter(r => r.id === 'dho.vote.sz')[0]).size
    })

  }

  const checkDhoVotes = async ({ expected, given, should }) => {

    const dhovotesTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'dhovotes',
      json: true
    })

    const actual = dhovotesTable.rows.map(r => {
      delete r.timestamp
      return r
    })

    assert({
      given,
      should,
      expected,
      actual: actual
    })

  }

  const checkDhoShares = async ({ expected, given, should }) => {

    const dhosharesTable = await getTableRows({
      code: dao,
      scope: dao,
      table: 'dhoshares',
      json: true
    })

    console.log(dhosharesTable)

    assert({
      given,
      should,
      expected,
      actual: dhosharesTable.rows
    })

  }


  console.log('add dhos')
  await Promise.all(orgs.map(org => contracts.dao.createdho(org, { authorization: `${org}@active` })))

  console.log('remove dho 6')
  await contracts.dao.removedho(orgs[5], { authorization: `${dao}@active` })
  
  orgs = orgs.slice(0, 5)
  const [org1, org2, org3, org4, org5] = orgs

  console.log('vote for dhos')
  await contracts.dao.votedhos(seconduser, [
    { dho: org1, points: 100 }
  ], { authorization: `${seconduser}@active` })

  await contracts.dao.votedhos(thirduser, [
    { dho: org1, points: 50 },
    { dho: org2, points: 50 }
  ], { authorization: `${thirduser}@active` })

  await contracts.dao.votedhos(fourthuser, [
    { dho: org1, points: 55 },
    { dho: org2, points: 20 },
    { dho: org3, points: 20 },
    { dho: org4, points: 5 },
  ], { authorization: `${fourthuser}@active` })


  await checkDhos({
    given: 'users voted for dhos',
    should: 'have the correct votes',
    expected: [
      { org_name: org1, points: 3650 },
      { org_name: org2, points: 1600 },
      { org_name: org3, points: 600 },
      { org_name: org4, points: 150 },
      { org_name: org5, points: 0 } 
    ]
  })

  await checkDhoVotes({
    given: 'users voted for dhos',
    should: 'have the correct entries in the dho votes table',
    expected: [
      { vote_id: 0, account: seconduser, dho: org1, points: 1000 },
      { vote_id: 1, account: thirduser, dho: org1, points: 1000 },
      { vote_id: 2, account: thirduser, dho: org2, points: 1000 },
      { vote_id: 3, account: fourthuser, dho: org1, points: 1650 },
      { vote_id: 4, account: fourthuser, dho: org2, points: 600 },
      { vote_id: 5, account: fourthuser, dho: org3, points: 600 },
      { vote_id: 6, account: fourthuser, dho: org4, points: 150 }
    ]
  })

  console.log('calc dho distribution')
  await contracts.dao.dhocalcdists({ authorization: `${dao}@active` })

  await checkDhoShares({
    given: 'distribution calculated',
    should: 'have the correct distribution entries',
    expected: [
      {
        dho: 'org1',
        total_percentage: '0.60833333333333328',
        dist_percentage: '0.62393162393162394'
      },
      {
        dho: 'org2',
        total_percentage: '0.26666666666666666',
        dist_percentage: '0.27350427350427353'
      },
      {
        dho: 'org3',
        total_percentage: '0.10000000000000001',
        dist_percentage: '0.10256410256410256'
      }
    ]
  })

  console.log('---------- recasting votes ----------')
  console.log('configure the voice recasting period to be 3 seconds')

  await contracts.settings.configure('dho.v.recast', 3, { authorization: `${settings}@active` })
  await sleep(4000)

  console.log('recast the votes before getting deleted')
  await contracts.dao.votedhos(thirduser, [
    { dho: org1, points: 25 },
    { dho: org2, points: 75 }
  ], { authorization: `${thirduser}@active` })

  console.log('clean the old votes')
  await contracts.dao.dhocleanvts({ authorization: `${dao}@active` })
  await sleep(5000)

  await checkDhos({
    given: 'votes cleaned',
    should: 'leave only the ones that are not old enough',
    expected: [
      { org_name: org1, points: 500 },
      { org_name: org2, points: 1500 },
      { org_name: org3, points: 0 },
      { org_name: org4, points: 0 },
      { org_name: org5, points: 0 }
    ]
  })

  await checkDhoVotes({
    given: 'votes cleaned',
    should: 'only leave the votes that are not old enough',
    expected: [
      { vote_id: 7, account: thirduser, dho: org1, points: 500 },
      { vote_id: 8, account: thirduser, dho: org2, points: 1500 }
    ]
  })

  console.log('calc dho distribution')
  await contracts.dao.dhocalcdists({ authorization: `${dao}@active` })

  await checkDhoShares({
    given: 'votes cleaned',
    should: 'recalculate the sharings table',
    expected: [
      {
        dho: 'org1',
        total_percentage: '0.25000000000000000',
        dist_percentage: '0.25000000000000000'
      },
      {
        dho: 'org2',
        total_percentage: '0.75000000000000000',
        dist_percentage: '0.75000000000000000'
      }
    ]
  })

  console.log('---------- remove organization ----------')
  
  console.log('vote for dhos')
  await contracts.dao.votedhos(seconduser, [
    { dho: org1, points: 100 }
  ], { authorization: `${seconduser}@active` })

  await contracts.dao.votedhos(fourthuser, [
    { dho: org1, points: 55 },
    { dho: org2, points: 20 },
    { dho: org3, points: 20 },
    { dho: org4, points: 5 },
  ], { authorization: `${fourthuser}@active` })

  console.log('remove org 1')
  await contracts.dao.removedho(org1, { authorization: `${dao}@active` })
  await sleep(5000)

  await checkDhos({
    given: 'dho removed',
    should: 'have the correct entries in the dho table',
    expected: [
      { org_name: org2, points: 2100 },
      { org_name: org3, points: 600 },
      { org_name: org4, points: 150 },
      { org_name: org5, points: 0 }
    ]
  })

  await checkDhoVotes({
    given: 'dho removed',
    should: 'have the votes removed as well',
    expected: [
      { vote_id: 8, account: thirduser, dho: org2, points: 1500 },
      { vote_id: 11, account: fourthuser, dho: org2, points: 600 },
      { vote_id: 12, account: fourthuser, dho: org3, points: 600 },
      { vote_id: 13, account: fourthuser, dho: org4, points: 150 }
    ]
  })

  await checkDhoShares({
    given: 'dho removed',
    should: 'remove the dho from the shares tables',
    expected: [
      {
        dho: 'org2',
        total_percentage: '0.73684210526315785',
        dist_percentage: '0.77777777777777779'
      },
      {
        dho: 'org3',
        total_percentage: '0.21052631578947367',
        dist_percentage: '0.22222222222222221'
      }
    ]
  })

  console.log('---------- DHO vote delegation ----------')
  
  console.log('delegate vote')
  await contracts.dao.delegate(seconduser, thirduser, dhosScope, { authorization: `${seconduser}@active` })
  await contracts.dao.delegate(thirduser, fourthuser, dhosScope, { authorization: `${thirduser}@active` })

  console.log('vote for org5')
  await contracts.dao.votedhos(fourthuser, [
    { dho: org5, points: 100 }
  ], { authorization: `${fourthuser}@active` })
  await sleep(5000)

  let cannotVoteWhenVoiceDelegated = true
  try {
    await contracts.dao.votedhos(seconduser, [
      { dho: org1, points: 100 }
    ], { authorization: `${seconduser}@active` })
    cannotVoteWhenVoiceDelegated = false
  } catch (error) {
    console.log('can not vote when voice is delegated (expected)')
  }

  await checkDhos({
    given: 'vote delegated',
    should: 'distribute the vote properly',
    expected: [
      { org_name: org2, points: 0 },
      { org_name: org3, points: 0 },
      { org_name: org4, points: 0 },
      { org_name: org5, points: 6000 }
    ]
  })

  await checkDhoVotes({
    given: 'vote delegated',
    should: 'have the correct entries in the dho votes table',
    expected: [
      {
        vote_id: 9,
        account: fourthuser,
        dho: org5,
        points: 3000
      },
      {
        vote_id: 10,
        account: thirduser,
        dho: org5,
        points: 2000
      },
      {
        vote_id: 11,
        account: seconduser,
        dho: org5,
        points: 1000
      }
    ]
  })

  assert({
    given: 'voice delegated',
    should: 'not be able to vote on their own',
    actual: cannotVoteWhenVoiceDelegated,
    expected: true
  })

  console.log('undelegate voice')
  await contracts.dao.undelegate(thirduser, dhosScope, { authorization: `${thirduser}@active` })

  console.log('vote for org2')
  await contracts.dao.votedhos(fourthuser, [
    { dho: org2, points: 100 }
  ], { authorization: `${fourthuser}@active` })
  await sleep(3000)

  await checkDhos({
    given: 'vote undelegated',
    should: 'distribute the vote properly',
    expected: [
      { org_name: org2, points: 3000 },
      { org_name: org3, points: 0 },
      { org_name: org4, points: 0 },
      { org_name: org5, points: 3000 }
    ]
  })

  await checkDhoVotes({
    given: 'vote undelegated',
    should: 'have the correct entries in the dhos vote table',
    expected: [
      {
        vote_id: 10,
        account: thirduser,
        dho: 'org5',
        points: 2000
      },
      {
        vote_id: 11,
        account: seconduser,
        dho: 'org5',
        points: 1000
      },
      {
        vote_id: 12,
        account: fourthuser,
        dho: 'org2',
        points: 3000
      }
    ]
  })

  const dhosTable5 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'dhos',
    json: true
  })
  console.log(dhosTable5)

  const dhovotesTable5 = await getTableRows({
    code: dao,
    scope: dao,
    table: 'dhovotes',
    json: true
  })
  console.log(dhovotesTable5)

})


describe('Cycle stats use moon phases', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }
  
  await resetContracts()

  const contracts = await initContracts({ dao, scheduler })

  console.log('reset scheduler')
  await contracts.scheduler.reset({ authorization: `${scheduler}@active` })

  const moonPhases = ['First Quarter', 'Full Moon', 'Last Quarter', 'New Moon']
  const now = parseInt(Date.now() / 1000)

  for (let i = 1; i <= 10; i++) {
    await contracts.scheduler.moonphase(now + (i * 86400), moonPhases[i%4], 0, { authorization: `${scheduler}@active` })
  }

  console.log('init propcycle')
  await contracts.dao.initcycle(1, { authorization: `${dao}@active` })

  await contracts.dao.onperiod({ authorization: `${dao}@active` })

  const cycleStatsTable = await getTableRows({
    code: dao,
    scope: dao,
    table: 'cyclestats',
    json: true
  })

  const moonPhasesTable = await getTableRows({
    code: scheduler,
    scope: scheduler,
    table: 'moonphases',
    limit: 1000,
    json: true
  })

  const cycleStat = cycleStatsTable.rows[0]
  const startTimePhase = moonPhasesTable.rows.filter(r => (r.timestamp == cycleStat.start_time))
  const endTimePhase = moonPhasesTable.rows.filter(r => (r.timestamp == cycleStat.end_time))

  assert({
    given: 'onperiod ran',
    should: 'set the start date and end date with a valid moon cycle',
    expected: [1, 1],
    actual: [startTimePhase.length, endTimePhase.length]
  })

  assert({
    given: 'onperiod ran',
    should: 'set end date bigger than start date',
    expected: true,
    actual: cycleStat.end_time > cycleStat.start_time
  })

  assert({
    given: 'onperiod ran',
    should: 'have the moon phase set to New Moon',
    expected: true,
    actual: startTimePhase[0].phase_name == 'New Moon' && endTimePhase[0].phase_name == 'New Moon'
  })

})
