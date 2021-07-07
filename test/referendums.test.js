const { describe } = require('riteway')
const { eos, names, getTableRows, initContracts, isLocal } = require('../scripts/helper')
const { should } = require('chai')

const { referendums, token, settings, accounts, firstuser, seconduser, thirduser, fourthuser } = names

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms))

function formatSpecialAttributes (referendums) {
  const refs = []

  for (const referendum of referendums) {
    const r = { ...referendum }
    delete r.special_attributes

    for (const attr of referendum.special_attributes) {
      r[attr.key] = attr.value[1]
    }

    refs.push(r)
  }

  return refs
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

  const contracts = await initContracts({ referendums, token, settings, accounts })

  console.log('reset referendums')
  await contracts.referendums.reset({ authorization: `${referendums}@active` })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  const createReferendum = async (creator, settingName, newValue, title, summary, description, image, url) => {
    await contracts.referendums.create([
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
      code: referendums,
      scope: referendums,
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
      code: referendums,
      scope: referendums,
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

    const vote = favour ? contracts.referendums.favour : contracts.referendums.against
    number = number == null ? users.length : number

    for (let i = 0; i < number; i++) {
      await vote(users[i], referendumId, 2, { authorization: `${users[i]}@active` })
    }

  }

  const revertVote = async (referendumId, number=null) => {

    number = number == null ? users.length : number

    for (let i = 0; i < number; i++) {
      await contracts.referendums.revertvote(users[i], referendumId, { authorization: `${users[i]}@active` })
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
  await contracts.referendums.initcycle(1, { authorization: `${referendums}@active` })

  console.log('insert test settings')
  await contracts.settings.confwithdesc(testSetting, 1, 'test description 1', highImpact, { authorization: `${settings}@active` })
  await contracts.settings.conffloatdsc(testSettingFloat, 1.2, 'test description 2', medImpact, { authorization: `${settings}@active` })

  console.log('join users')
  await Promise.all(users.map(user => contracts.accounts.adduser(user, user, 'individual', { authorization: `${accounts}@active` })))
  await Promise.all(users.map(user => contracts.accounts.testcitizen(user, { authorization: `${accounts}@active` })))
  await Promise.all(users.map(user => contracts.referendums.testsetvoice(user, 99, { authorization: `${referendums}@active` })))

  console.log('create referendum')
  await createReferendum(firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(firstuser, testSetting, ['uint64', 100], 'title', 'summary', 'description', 'image', 'url')
  await createReferendum(seconduser, testSettingFloat, ['float64', testSettingFloatNewValue], 'title 2', 'summary 2', 'description 2', 'image 2', 'url 2')

  console.log('update referendum')
  await contracts.referendums.update([
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
  await contracts.referendums.cancel([{ key: 'proposal_id', value: ['uint64', 1] }], { authorization: `${firstuser}@active` })

  const refTables = await getTableRows({
    code: referendums,
    scope: referendums,
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
  await contracts.token.transfer(firstuser, referendums, `${minStake}.0000 SEEDS`, '0', { authorization: `${firstuser}@active` })

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  await sleep(2000)

  console.log('vote favour for first referendum')
  await voteReferendum(0)

  console.log('staking for second referendum')
  await contracts.token.transfer(seconduser, referendums, `${minStake}.0000 SEEDS`, '2', { authorization: `${seconduser}@active` })

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
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
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
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
    await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
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
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
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
  await contracts.token.transfer(firstuser, referendums, `${minStake}.0000 SEEDS`, '3', { authorization: `${firstuser}@active` })
  await contracts.token.transfer(seconduser, referendums, `${minStake}.0000 SEEDS`, '4', { authorization: `${seconduser}@active` })
  await contracts.token.transfer(thirduser, referendums, `${minStake}.0000 SEEDS`, '5', { authorization: `${thirduser}@active` })
  await contracts.token.transfer(thirduser, referendums, `${minStake}.0000 SEEDS`, '6', { authorization: `${thirduser}@active` })

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  await sleep(2000)

  await voteReferendum(3)
  await voteReferendum(4)
  await voteReferendum(5, 4, false)
  await voteReferendum(6)

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  await sleep(2000)

  console.log('change trust for referendum 1')
  await revertVote(4, 3)

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  await sleep(2000)

  console.log('running onperiod')
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
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
  await contracts.referendums.onperiod({ authorization: `${referendums}@active` })
  await sleep(2000)

  const refTables2 = await getTableRows({
    code: referendums,
    scope: referendums,
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
