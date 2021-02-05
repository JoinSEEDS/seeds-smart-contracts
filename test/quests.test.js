const { describe } = require('riteway')
const R = require('ramda')
const { eos, names, getTableRows, getBalance, initContracts, isLocal, getBalanceFloat } = require('../scripts/helper');
const { expect, assert } = require('chai');
const { stat } = require('fs-extra');

const { quests, accounts, settings, escrow, token, firstuser, seconduser, thirduser, fourthuser, fifthuser, campaignbank, proposals } = names

const fixedDetails = 'fixed'
const variableDetails = 'variable'

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function getFixedNodes (account, type, useCreator=null) {

  const documents = await getTableRows({
    code: quests,
    scope: quests,
    table: 'documents',
    index_position: 3,
    key_type: 'i64',
    limit: 500
  })
  return documents.rows.filter(row => {
        
    const contentGroups = row.content_groups
    let isTheCorrectNode = 0

    if (useCreator) {
      if (row.creator == useCreator) {
        isTheCorrectNode++
      }
    }

    for (const contentGroup of contentGroups) {
      if (contentGroup[0].value[1] == 'fixed_details') {
        for (let i = 1; i < contentGroup.length; i++) {
          const { label, value } = contentGroup[i]
          if (label == 'type' && value[1] == type) { isTheCorrectNode++ }
        }
      } else if (contentGroup[0].value[1] == 'identifier_details') {
        for (let i = 1; i < contentGroup.length; i++) {
          const { label, value } = contentGroup[i]
          if (label == 'owner' && value[1] == account) { isTheCorrectNode++ }
        }
      }
    }

    if (isTheCorrectNode >= 2) { return true }
    return false

  })

}

async function getNodesTo (hash, edgeName, verbose=false) {

  const edges = await getTableRows({
    code: quests,
    scope: quests,
    table: 'edges',
    index_position: 2,
    key_type: 'sha256',
    lower_bound: hash,
    upper_bound: hash,
    limit: 500
  })

  const filterEdges = edges.rows.filter(row => row.edge_name == edgeName)
  const nodes = []

  if (verbose) {
    console.log('edges:', edges)
    console.log('filterEdges:', filterEdges)
  }

  for (const edge of filterEdges) {
    const document = await getTableRows({
      code: quests,
      scope: quests,
      table: 'documents',
      index_position: 2,
      key_type: 'sha256',
      lower_bound: edge.to_node,
      upper_bound: edge.to_node,
      limit: 500
    })
    nodes.push(document.rows[0])
  }

  return nodes

}

async function getFixedVariables (nodesFixed, verbose=false) {
  nodes = []
  for (const nodeFixed of nodesFixed) {
    nodes.push({
      fixed: nodeFixed,
      variable: (await getNodesTo(nodeFixed.hash, 'variable', verbose))[0]
    })
  }
  return nodes
}

async function getRootBalance () {
  const { rows } = await getTableRows({
    code: quests,
    scope: quests,
    table: 'documents'
  })
  for (let i = 0; i < rows.length; i++) {
    const contentGroups = rows[i].content_groups
    for (const contentGroup of contentGroups) {
      if (contentGroup[0].value[1] == 'fixed_details') {
        for (let i = 1; i < contentGroup.length; i++) {
          const { label, value } = contentGroup[i]
          if (label == 'type' && value[1] == 'accntinfos') { 
            return {
              fixed: rows[i],
              variable: (await getNodesTo(rows[i].hash, 'variable'))[0]
            }
          }
        }
      }
    }
  }

  return null
}

async function getQuests (account, verbose=false) {
  return await getFixedVariables(await getFixedNodes(account, 'quest'), verbose)
}

async function getAccountInfo (account) {
  const result = await getFixedVariables(await getFixedNodes(account, 'accntinfo'))
  return result[0]
}

async function getMilestones (questHash) {
  const milestonesFixed = await getNodesTo(questHash, 'hasmilestone')
  return await getFixedVariables(milestonesFixed)
}

async function getApplicants (questHash) {
  return await getFixedVariables(await getNodesTo(questHash, 'hasapplicant'))
}

async function getMaker (questHash) {
  const makerFixed = await getNodesTo(questHash, 'hasmaker')
  return (await getFixedVariables(makerFixed))[0]
}

async function getProposals (label=null, value=null) {
  const proposalsFixed = await getFixedNodes(quests, 'proposal', quests)
  let nodes = await getFixedVariables(proposalsFixed)
  if (label != null) {
    nodes = nodes.filter(node => {
      const variable = node.variable
      for (const contentGroup of variable.content_groups) {
        for (const content of contentGroup) {
          if (content.label == label && content.value[1] == value) {
            return true
          }
        }
      }
      return false
    })
  }
  return nodes
}

function getValueFromNode (node, contentGroupLabel, contentLabel) {
  const contentGroups = node.content_groups
  for (const contentGroup of contentGroups) {
    if (contentGroup[0].value[1] == contentGroupLabel) {
      for (const content of contentGroup) {
        if (content.label == contentLabel) {
          return content.value[1]
        }
      }
    }
  }
  return null
}


const checkNodeValue = async (assert, { node, label, value, given, should }, details) => {
  let nodeDetails
  let labelDetails
  if (details == 'fixed') {
    nodeDetails = node.fixed
    labelDetails = 'fixed_details'
  } else if (details == 'variable') {
    nodeDetails = node.variable
    labelDetails = 'variable_details'
  }
  const currentValue = getValueFromNode(nodeDetails, labelDetails, label)
  assert({
    given,
    should,
    actual: currentValue,
    expected: value
  })
}

const checkNodes = async (assert, details, { nodes, label, value, given, should }, numberOfNodes=null) => {
  let numberN = 0
  const valueIsArray = Array.isArray(value)
  nodes = Array.isArray(nodes) ? nodes : [nodes]
  for (const node of nodes) {
    await checkNodeValue(assert, {
      node,
      label,
      value: valueIsArray ? value[numberN] : value,
      given,
      should
    }, details)
    numberN++
  }
  if (numberOfNodes != null){
    assert({
      given: 'number of nodes',
      should: 'have the correct number',
      actual: numberN,
      expected: numberOfNodes
    })
  }
}

const checkRootBalance = async (assert, expectedAmount) => {
  const rootBalance = await getRootBalance()
  assert({
    given: 'seeds moved in quests contract',
    should: 'have the correct balance',
    actual: rootBalance.variable.content_groups[0][1].value[1],
    expected: expectedAmount
  })
}


// describe('Private quests', async assert => {

//   if (!isLocal()) {
//     console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
//     return
//   }

//   const contracts = await initContracts({ quests, accounts, token, settings, escrow })

//   console.log('reset settings')
//   await contracts.settings.reset({ authorization: `${settings}@active` })

//   console.log('reset accounts')
//   await contracts.accounts.reset({ authorization: `${accounts}@active` })

//   console.log('reset escrow')
//   await contracts.escrow.reset({ authorization: `${escrow}@active` })

//   console.log('reset quests')
//   await contracts.quests.reset({ authorization: `${quests}@active` })

//   console.log('join users')
//   await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })

//   console.log('add private quest')
//   await contracts.quests.addquest(seconduser, '100.0000 SEEDS', 'Test quest 1', 'Test quest 1', seconduser, 1, { authorization: `${seconduser}@active` })
//   await contracts.quests.addquest(firstuser, '100.0000 SEEDS', 'Test quest 2', 'Test quest 2', firstuser, 1, { authorization: `${firstuser}@active` })

//   const firstuserQuests = await getQuests(firstuser)
//   const firstuserQ1 = firstuserQuests[0]

//   console.log('add milestones')
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 1', 'milestone description', 2000, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 2', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 3', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 4', 'milestone description', 3900, { authorization: `${firstuser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(firstuserQ1.fixed.hash),
//     label: 'status',
//     value: 'notcompleted',
//     given: 'milestones created',
//     should: 'have milestones'
//   }, 4)

//   console.log('transfer to quests')
//   await contracts.token.transfer(firstuser, quests, '50.0000 SEEDS', 'my quest supply', { authorization: `${firstuser}@active` })
//   await checkRootBalance(assert, '50.0000 SEEDS')
 
//   console.log('activate quest')
//   let activeQuestsNoBalance = false
//   try {
//     await contracts.quests.activate(firstuserQ1.fixed.hash, { authorization: `${firstuser}@active` })
//     activeQuestsNoBalance = true
//   } catch (err) {
//     console.log('not enough balance to active the quest (expected)')
//   }

//   await sleep(200)
//   await contracts.token.transfer(firstuser, quests, '100.0000 SEEDS', 'my quest supply', { authorization: `${firstuser}@active` })
//   await contracts.quests.activate(firstuserQ1.fixed.hash, { authorization: `${firstuser}@active` })

//   await checkRootBalance(assert, '150.0000 SEEDS')

//   await checkNodes(assert, variableDetails, {
//     nodes: [await getAccountInfo(firstuser)],
//     label: 'account_balance',
//     value: '50.0000 SEEDS',
//     given: 'transfer to quests',
//     should: 'have the correct balance'
//   })

//   await checkNodes(assert, variableDetails, {
//     nodes: [await getAccountInfo(firstuser)],
//     label: 'locked_balance',
//     value: '100.0000 SEEDS',
//     given: 'activate the quest',
//     should: 'have the correct locked balance'
//   })

//   console.log('withdraw balance')
//   const firstuserSeedsBalanceBefore = await getBalanceFloat(firstuser)
//   await contracts.quests.withdraw(firstuser, '25.0000 SEEDS', { authorization: `${firstuser}@active` })
//   const firstuserSeedsBalanceAfter = await getBalanceFloat(firstuser)

//   await checkNodes(assert, variableDetails, {
//     nodes: [await getAccountInfo(firstuser)],
//     label: 'account_balance',
//     value: '25.0000 SEEDS',
//     given: 'transfer to quests',
//     should: 'have the correct balance'
//   })
  
//   console.log('apply for a quest')
//   await contracts.quests.apply(firstuserQ1.fixed.hash, thirduser, 'Applicant description', { authorization: `${thirduser}@active` })
//   await contracts.quests.apply(firstuserQ1.fixed.hash, fourthuser, 'Applicant description', { authorization: `${fourthuser}@active` })

//   // list the applicants
//   const questApplicants = await getApplicants(firstuserQ1.fixed.hash)

//   await checkNodes(assert, variableDetails, {
//     nodes: questApplicants,
//     label: 'status',
//     value: 'pending',
//     given: 'applicants for a quest',
//     should: 'have the correct status'
//   }, 2)

//   console.log('accept applicant')
//   await contracts.quests.accptapplcnt(questApplicants[0].fixed.hash, { authorization: `${firstuser}@active` })

//   let onlyOneMaker = true
//   try {
//     await contracts.quests.accptapplcnt(questApplicants[1].fixed.hash, { authorization: `${firstuser}@active` })
//     onlyOneMaker = false
//   } catch (err) {
//     console.log('only one maker (expected)')
//   }

//   const questMaker = await getMaker(firstuserQ1.fixed.hash)

//   await checkNodes(assert, variableDetails, {
//     nodes: [questMaker],
//     label: 'status',
//     value: 'accepted',
//     given: 'applicant accepted',
//     should: 'have the correct status'
//   }, 1)

//   console.log('accept quest')
//   await contracts.quests.accptquest(firstuserQ1.fixed.hash, { authorization: `${questMaker.fixed.creator}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: [await getMaker(firstuserQ1.fixed.hash)],
//     label: 'status',
//     value: 'confirmed',
//     given: 'applicant accepted the quest',
//     should: 'have the correct status'
//   }, 1)

//   const activeMilestones = await getMilestones(firstuserQ1.fixed.hash)

//   await checkNodes(assert, variableDetails, {
//     nodes: activeMilestones,
//     label: 'status',
//     value: 'notcompleted',
//     given: 'applicant accepted the quest',
//     should: 'active the milestones'
//   }, 4)

//   console.log('milestone completed')
//   await contracts.quests.mcomplete(activeMilestones[0].fixed.hash, 'url', 'description', { authorization: `${questMaker.fixed.creator}@active` })
  
//   await checkNodes(assert, variableDetails, {
//     nodes: (await getMilestones(firstuserQ1.fixed.hash))[0],
//     label: 'status',
//     value: 'completed',
//     given: 'applicant completed one milestone',
//     should: 'have the correct status'
//   })
  
//   console.log('reject milestone')
//   await contracts.quests.rejctmilstne(activeMilestones[0].fixed.hash, { authorization: `${firstuser}@active` })
  
//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(firstuserQ1.fixed.hash),
//     label: 'status',
//     value: 'notcompleted',
//     given: 'owner rejected the milestone',
//     should: 'have the correct status'
//   }, 4)

//   let payoutAfterCompletion = true
//   try {
//     await contracts.quests.payoutmilstn(activeMilestones[0].fixed.hash, { authorization: `${firstuser}@active` })
//     payoutAfterCompletion = false
//   } catch (err) {
//     console.log('the milestone is not finished (expected)')
//   }

//   console.log('milestone finished')
//   await contracts.quests.mcomplete(activeMilestones[0].fixed.hash, 'url', 'fixed milestone', { authorization: `${questMaker.fixed.creator}@active` })
//   await contracts.quests.accptmilstne(activeMilestones[0].fixed.hash, { authorization: `${firstuser}@active` })
//   await contracts.quests.payoutmilstn(activeMilestones[0].fixed.hash, { authorization: `${firstuser}@active` })

//   let payoutOnce = true
//   try {
//     await contracts.quests.payoutmilstn(activeMilestones[0].fixed.hash, { authorization: `${firstuser}@active` })
//     payoutOnce = false
//   } catch (err) {
//     console.log('the milestone has been paid out (expected)')
//   }

//   await checkNodes(assert, variableDetails, {
//     nodes: (await getMilestones(firstuserQ1.fixed.hash))[0],
//     label: 'status',
//     value: 'finished',
//     given: 'owner accepted the milestone',
//     should: 'have the correct status'
//   })
//   await checkNodes(assert, variableDetails, {
//     nodes: (await getMilestones(firstuserQ1.fixed.hash))[0],
//     label: 'description',
//     value: 'fixed milestone',
//     given: 'maker completed the milestone',
//     should: 'have the correct description'
//   })

//   console.log('finish quest')
//   let i = 1
//   for (; i < activeMilestones.length; i++) {
//     await checkNodes(assert, variableDetails, {
//       nodes: (await getQuests(firstuser))[0],
//       label: 'stage',
//       value: 'active',
//       given: `maker completed the milestone ${i}`,
//       should: 'not finish the quest'
//     })

//     await contracts.quests.mcomplete(activeMilestones[i].fixed.hash, 'url', 'fixed milestone', { authorization: `${questMaker.fixed.creator}@active` })
//     await contracts.quests.accptmilstne(activeMilestones[i].fixed.hash, { authorization: `${firstuser}@active` })
//     await contracts.quests.payoutmilstn(activeMilestones[i].fixed.hash, { authorization: `${firstuser}@active` })
//   }

//   await checkNodes(assert, variableDetails, {
//     nodes: (await getQuests(firstuser))[0],
//     label: 'stage',
//     value: 'done',
//     given: `maker completed the milestone ${i}`,
//     should: 'finish the quest'
//   })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(firstuserQ1.fixed.hash),
//     label: 'status',
//     value: 'finished',
//     given: 'all the milestones finished',
//     should: 'have the correct status'
//   }, 4)

//   const questLocks = await getTableRows({
//     code: escrow,
//     scope: escrow,
//     table: 'locks',
//   })
//   console.log(questLocks)

//   await checkRootBalance(assert, '25.0000 SEEDS')

//   console.log('rate applicant')
//   await contracts.quests.rateapplcnt(questMaker.fixed.hash, 'like', { authorization: `${firstuser}@active` })
//   await sleep(200)
//   await contracts.quests.ratequest(firstuserQ1.fixed.hash, 'dislike', { authorization: `${questMaker.fixed.creator}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMaker(firstuserQ1.fixed.hash),
//     label: 'owner_opinion',
//     value: 'like',
//     given: 'applicant rated',
//     should: 'have the correct opinion'
//   })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getQuests(firstuser),
//     label: 'applicant_opinion',
//     value: 'dislike',
//     given: 'quest rated',
//     should: 'have the correct opinion'
//   })

//   assert({
//     given: 'call withdraw action',
//     should: 'receive the correct amount',
//     actual: firstuserSeedsBalanceAfter - firstuserSeedsBalanceBefore,
//     expected: 25
//   })

//   assert({
//     given: 'quest has a maker',
//     should: 'not accept another one',
//     actual: onlyOneMaker,
//     expected: true
//   })

//   assert({
//     given: 'milestone not completed',
//     should: 'not pay it out',
//     actual: payoutAfterCompletion,
//     expected: true
//   })

//   assert({
//     given: 'milestone paied out',
//     should: 'not pay twice',
//     actual: payoutOnce,
//     expected: true
//   })

// })


// describe('Voted quests', async assert => {

//   if (!isLocal()) {
//     console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
//     return
//   }

//   const contracts = await initContracts({ quests, accounts, token, settings, escrow, proposals })

//   console.log('reset settings')
//   await contracts.settings.reset({ authorization: `${settings}@active` })

//   console.log('reset accounts')
//   await contracts.accounts.reset({ authorization: `${accounts}@active` })

//   console.log('reset proposals')
//   await contracts.proposals.reset({ authorization: `${proposals}@active` })

//   console.log('reset escrow')
//   await contracts.escrow.reset({ authorization: `${escrow}@active` })

//   console.log('reset quests')
//   await contracts.quests.reset({ authorization: `${quests}@active` })

//   console.log('join users')
//   await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(fifthuser, 'fifthuser', 'individual', { authorization: `${accounts}@active` })

//   await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
//   await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
//   await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })
//   await contracts.accounts.testcitizen(fourthuser, { authorization: `${accounts}@active` })
//   await contracts.accounts.testcitizen(fifthuser, { authorization: `${accounts}@active` })

//   console.log('running on proposals::onperiod')
//   await contracts.proposals.onperiod({ authorization: `${proposals}@active` })
//   await sleep(3000)
//   await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
//   await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
//   await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })
//   await contracts.proposals.addvoice(fourthuser, 44, { authorization: `${proposals}@active` })
//   await contracts.proposals.addvoice(fifthuser, 44, { authorization: `${proposals}@active` })

//   console.log('add private quest')
//   await contracts.quests.addquest(firstuser, '100.0000 SEEDS', 'Test quest 2', 'Test quest 2', campaignbank, 1, { authorization: `${firstuser}@active` })
//   await contracts.quests.addquest(seconduser, '200.0000 SEEDS', 'Test quest 1', 'Test quest 1', campaignbank, 1, { authorization: `${seconduser}@active` })
  
//   await sleep(400)

//   const firstuserQuests = await getQuests(firstuser)
//   const firstuserQuest = firstuserQuests[0]

//   const seconduserQuests = await getQuests(seconduser)
//   const seconduserQuest = seconduserQuests[0]

//   console.log('add milestones')
//   await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 1', 'milestone description', 2000, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 2', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 3', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 4', 'milestone description', 3900, { authorization: `${firstuser}@active` })

//   await contracts.quests.addmilestone(seconduserQuest.fixed.hash, 'Test milestone title 1', 'milestone description', 10000, { authorization: `${seconduser}@active` })
//   await contracts.quests.addmilestone(seconduserQuest.fixed.hash, 'Test milestone title 2', 'milestone description', 8000, { authorization: `${seconduser}@active` })

//   console.log('delete milestone')
//   const ms = await getMilestones(seconduserQuest.fixed.hash)
//   await contracts.quests.delmilestone(ms[1].fixed.hash, { authorization: `${seconduser}@active` })

//   console.log('propose quest')
//   await contracts.quests.activate(seconduserQuest.fixed.hash, { authorization: `${seconduser}@active` })
//   await contracts.quests.activate(firstuserQuest.fixed.hash, { authorization: `${firstuser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: [(await getQuests(firstuser))[0], (await getQuests(seconduser))[0]],
//     label: 'stage',
//     value: 'proposed',
//     given: `quest proposed`,
//     should: 'have the correct stage'
//   })

//   await checkNodes(assert, variableDetails, { 
//     nodes: await getProposals(),
//     label: 'stage',
//     value: 'staged',
//     given: 'quests proposed',
//     should: 'have proposals'
//   }, 2)

//   console.log('active proposals')
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(5000)

//   await checkNodes(assert, variableDetails, { 
//     nodes: await getProposals(),
//     label: 'stage',
//     value: 'active',
//     given: 'onperiod ran',
//     should: 'have the correct stage'
//   }, 2)

//   const proposals1 = await getProposals()

//   console.log('vote for proposals')
//   // await contracts.quests.favour(seconduser, proposals[0].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await contracts.quests.favour(thirduser, proposals1[0].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fourthuser, proposals1[0].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fifthuser, proposals1[0].fixed.hash, 7, { authorization: `${fifthuser}@active` })
//   await sleep(300)

//   await contracts.quests.against(seconduser, proposals1[1].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await sleep(300)
//   await contracts.quests.against(thirduser, proposals1[1].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fourthuser, proposals1[1].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fifthuser, proposals1[1].fixed.hash, 7, { authorization: `${fifthuser}@active` })
//   await sleep(300)
  
//   console.log('evaluate proposals')
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(5000)

//   await checkNodes(assert, variableDetails, { 
//     nodes: await getProposals(),
//     label: 'status',
//     value: ['passed', 'rejected'],
//     given: 'onperiod ran',
//     should: 'have the correct status'
//   }, 2)

//   await checkNodes(assert, variableDetails, { 
//     nodes: await getProposals(),
//     label: 'stage',
//     value: 'done',
//     given: 'onperiod ran',
//     should: 'have the correct stage'
//   }, 2)

//   // console.log((await getQuests(firstuser, true))[0], (await getQuests(seconduser, true))[0])
 
//   await checkNodes(assert, variableDetails, {
//     nodes: [(await getQuests(firstuser))[0], (await getQuests(seconduser))[0]],
//     label: 'stage',
//     value: ['staged', 'active'],
//     given: `quest proposed`,
//     should: 'have the correct stage'
//   })

//   await checkNodes(assert, variableDetails, {
//     nodes: [(await getQuests(firstuser))[0], (await getQuests(seconduser))[0]],
//     label: 'status',
//     value: 'open',
//     given: `quest proposed`,
//     should: 'have the correct stage'
//   })

//   const quest = (await getQuests(seconduser))[0]

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(quest.fixed.hash),
//     label: 'status',
//     value: 'notcompleted',
//     given: `quest passed the proposal`,
//     should: 'have the milestones not started'
//   })

//   console.log('apply for a quest')
//   await contracts.quests.apply(quest.fixed.hash, firstuser, 'Applicant description', { authorization: `${firstuser}@active` })
//   await contracts.quests.apply(quest.fixed.hash, thirduser, 'Applicant description', { authorization: `${thirduser}@active` })

//   const questApplicants = await getApplicants(quest.fixed.hash)

//   await checkNodes(assert, fixedDetails, {
//     nodes: questApplicants,
//     label: 'type',
//     value: 'applicant',
//     given: 'applicants for a quest',
//     should: 'have applicants'
//   }, 2)

//   await sleep(400)

//   console.log('activate proposals')
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(3000)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getProposals('status', 'open'),
//     label: 'status',
//     value: 'open',
//     given: 'applicants for a voted quest',
//     should: 'have proposals'
//   }, 2)

//   console.log('vote for participants')
//   const proposals2 = await getProposals('status', 'open')

//   await sleep(300)

//   await contracts.quests.favour(seconduser, proposals2[0].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(thirduser, proposals2[0].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fourthuser, proposals2[0].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fifthuser, proposals2[0].fixed.hash, 7, { authorization: `${fifthuser}@active` })
//   await sleep(300)

//   let onlyValidators = true
//   try {
//     await sleep(300)
//     await contracts.quests.favour(firstuser, proposals2[0].fixed.hash, 7, { authorization: `${firstuser}@active` })
//     onlyValidators = false
//   } catch (err) {
//     console.log('only validators can vote (expected)')
//   }

//   await sleep(300)

//   await contracts.quests.favour(seconduser, proposals2[1].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(thirduser, proposals2[1].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fourthuser, proposals2[1].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fifthuser, proposals2[1].fixed.hash, 7, { authorization: `${fifthuser}@active` })
//   await sleep(300)

//   console.log('evaluate proposals')
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(3000)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMaker(quest.fixed.hash),
//     label: 'status',
//     value: 'accepted',
//     given: 'applicant accepted',
//     should: 'have one maker'
//   }, 1)

//   const questMaker = await getMaker(quest.fixed.hash)

//   console.log('accept the quest')
//   await contracts.quests.accptquest(quest.fixed.hash, { authorization: `${questMaker.fixed.creator}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: [await getMaker(quest.fixed.hash)],
//     label: 'status',
//     value: 'confirmed',
//     given: 'applicant accepted the quest',
//     should: 'have the correct status'
//   }, 1)

//   console.log('reject milestone')
//   const activeMilestones = await getMilestones(quest.fixed.hash)
//   await contracts.quests.mcomplete(activeMilestones[0].fixed.hash, 'url', 'description', { authorization: `${questMaker.fixed.creator}@active` })

//   await sleep(200)
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(4000)

//   const proposals3 = await getProposals('status', 'open')

//   await contracts.quests.against(seconduser, proposals3[0].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await sleep(300)
//   await contracts.quests.against(thirduser, proposals3[0].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.against(fourthuser, proposals3[0].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.against(fifthuser, proposals3[0].fixed.hash, 7, { authorization: `${fifthuser}@active` })

//   await sleep(200)

//   console.log('evaluate proposals')
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   await sleep(4000)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(quest.fixed.hash),
//     label: 'status',
//     value: 'notcompleted',
//     given: 'applicant complete the milestone badly',
//     should: 'reject the milestone'
//   }, 1)

//   console.log('accept milestone')
//   await contracts.quests.mcomplete(activeMilestones[0].fixed.hash, 'url', 'description 1', { authorization: `${questMaker.fixed.creator}@active` })
//   console.log('mcomplete... done')
//   await sleep(200)

//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
//   console.log('onperiod... done')
//   await sleep(4000)

//   const proposals4 = await getProposals('status', 'open')

//   await contracts.quests.favour(seconduser, proposals4[0].fixed.hash, 7, { authorization: `${seconduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(thirduser, proposals4[0].fixed.hash, 7, { authorization: `${thirduser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fourthuser, proposals4[0].fixed.hash, 7, { authorization: `${fourthuser}@active` })
//   await sleep(300)
//   await contracts.quests.favour(fifthuser, proposals4[0].fixed.hash, 7, { authorization: `${fifthuser}@active` })
//   console.log('voting... done')

//   console.log('evaluate proposals')
//   await sleep(200)
//   await contracts.quests.onperiod({ authorization: `${quests}@active` })
  
//   await sleep(5000)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(quest.fixed.hash),
//     label: 'status',
//     value: 'finished',
//     given: 'applicant complete the milestone well',
//     should: 'accept the milestone'
//   }, 1)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getQuests(seconduser),
//     label: 'stage',
//     value: 'done',
//     given: 'applicant complete the milestone well',
//     should: 'finish the quest'
//   }, 1)

//   await checkNodes(assert, variableDetails, {
//     nodes: await getQuests(seconduser),
//     label: 'status',
//     value: 'finished',
//     given: 'applicant complete the milestone well',
//     should: 'finish the quest'
//   }, 1)

//   console.log('payout milestone')
//   await contracts.quests.payoutmilstn(activeMilestones[0].fixed.hash, { authorization: `${fifthuser}@active` })
//   await sleep(300)

//   const questLocks = await getTableRows({
//     code: escrow,
//     scope: escrow,
//     table: 'locks',
//   })
//   console.log(questLocks)

//   assert({
//     given: 'proposals are voted',
//     should: 'allow only the validators',
//     actual: onlyValidators,
//     expected: true
//   })

// })


// describe('Private quests timeouts', async assert => {

//   if (!isLocal()) {
//     console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
//     return
//   }

//   const contracts = await initContracts({ quests, accounts, token, settings, escrow })

//   console.log('reset settings')
//   await contracts.settings.reset({ authorization: `${settings}@active` })

//   console.log('reset accounts')
//   await contracts.accounts.reset({ authorization: `${accounts}@active` })

//   console.log('reset escrow')
//   await contracts.escrow.reset({ authorization: `${escrow}@active` })

//   console.log('reset quests')
//   await contracts.quests.reset({ authorization: `${quests}@active` })

//   console.log('join users')
//   await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })
//   await contracts.accounts.adduser(fifthuser, 'fifthuser', 'individual', { authorization: `${accounts}@active` })

//   console.log('add private quest')
//   await contracts.quests.addquest(seconduser, '100.0000 SEEDS', 'Test quest 1', 'Test quest 1', seconduser, 1, { authorization: `${seconduser}@active` })
//   await contracts.quests.addquest(firstuser, '100.0000 SEEDS', 'Test quest 2', 'Test quest 2', firstuser, 1, { authorization: `${firstuser}@active` })

//   const firstuserQuests = await getQuests(firstuser)
//   const firstuserQ1 = firstuserQuests[0]

//   const seconduserQuests = await getQuests(seconduser)
//   const seconduserQ1 = seconduserQuests[0]

//   console.log('add milestones')
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 1', 'milestone description', 2000, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 2', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 3', 'milestone description', 2050, { authorization: `${firstuser}@active` })
//   await contracts.quests.addmilestone(firstuserQ1.fixed.hash, 'Test milestone title 4', 'milestone description', 3900, { authorization: `${firstuser}@active` })

//   await contracts.quests.addmilestone(seconduserQ1.fixed.hash, 'Test milestone title 1', 'milestone description', 5000, { authorization: `${seconduser}@active` })
//   await contracts.quests.addmilestone(seconduserQ1.fixed.hash, 'Test milestone title 2', 'milestone description', 5000, { authorization: `${seconduser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getMilestones(firstuserQ1.fixed.hash),
//     label: 'status',
//     value: 'notcompleted',
//     given: 'milestones created',
//     should: 'have milestones'
//   }, 4)

//   console.log('transfer to quests')
//   await contracts.token.transfer(firstuser, quests, '100.0000 SEEDS', 'my quest supply', { authorization: `${firstuser}@active` })
//   await contracts.token.transfer(seconduser, quests, '100.0000 SEEDS', 'my quest supply', { authorization: `${seconduser}@active` })
//   await checkRootBalance(assert, '200.0000 SEEDS')
 
//   console.log('activate quest')
//   await sleep(200)
//   await contracts.quests.activate(firstuserQ1.fixed.hash, { authorization: `${firstuser}@active` })
//   await contracts.quests.activate(seconduserQ1.fixed.hash, { authorization: `${seconduser}@active` })

//   console.log('apply for a quest')
//   await contracts.quests.apply(firstuserQ1.fixed.hash, thirduser, 'Applicant description', { authorization: `${thirduser}@active` })
//   await contracts.quests.apply(firstuserQ1.fixed.hash, fourthuser, 'Applicant description', { authorization: `${fourthuser}@active` })

//   await contracts.quests.apply(seconduserQ1.fixed.hash, thirduser, 'Applicant description', { authorization: `${thirduser}@active` })
//   await contracts.quests.apply(seconduserQ1.fixed.hash, fourthuser, 'Applicant description', { authorization: `${fourthuser}@active` })

//   console.log('accept applicant')
//   const firstuserQuestApplicants = await getApplicants(firstuserQ1.fixed.hash)
//   await contracts.quests.accptapplcnt(firstuserQuestApplicants[0].fixed.hash, { authorization: `${firstuser}@active` })
//   await sleep(1000)

//   console.log('expire applicant')
//   await contracts.settings.configure('qst.exp.appl', 1, { authorization: `${settings}@active` })
//   await sleep(2000)

//   await contracts.quests.expireappl(firstuserQuestApplicants[0].fixed.hash, { authorization: `${firstuser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: (await getApplicants(firstuserQ1.fixed.hash))[0],
//     label: 'status',
//     value: 'expired',
//     given: 'expired applicant',
//     should: 'have the correct status'
//   })

//   console.log('cancel applicant')
//   const seconduserQuestApplicants = await getApplicants(seconduserQ1.fixed.hash)
//   await contracts.quests.accptapplcnt(seconduserQuestApplicants[1].fixed.hash, { authorization: `${seconduser}@active` })
//   await contracts.quests.accptquest(seconduserQ1.fixed.hash, { authorization: `${seconduserQuestApplicants[1].fixed.creator}@active` })

//   await sleep(1000)

//   await contracts.quests.cancelappl(seconduserQuestApplicants[1].fixed.hash, { authorization: `${seconduser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: (await getApplicants(seconduserQ1.fixed.hash))[1],
//     label: 'status',
//     value: 'cancel',
//     given: 'canceled applicant',
//     should: 'have the correct status'
//   })

//   console.log('retract application')
//   await contracts.quests.retractappl(firstuserQuestApplicants[1].fixed.hash, { authorization: `${firstuserQuestApplicants[1].fixed.creator}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getApplicants(firstuserQ1.fixed.hash),
//     label: 'status',
//     value: 'expired',
//     given: 'applicant retracted',
//     should: 'delete its application'
//   }, 1)

//   console.log('quit applicant')
//   await contracts.quests.accptapplcnt(seconduserQuestApplicants[0].fixed.hash, { authorization: `${seconduser}@active` })
//   await contracts.quests.accptquest(seconduserQ1.fixed.hash, { authorization: `${seconduserQuestApplicants[0].fixed.creator}@active` })

//   await sleep(1000)
//   await contracts.quests.quitapplcnt(seconduserQuestApplicants[0].fixed.hash, { authorization: `${seconduserQuestApplicants[0].fixed.creator}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: (await getApplicants(seconduserQ1.fixed.hash))[0],
//     label: 'status',
//     value: 'quitted',
//     given: 'applicant given up',
//     should: 'have the correct status'
//   }, 1)

//   console.log('expire quest')
//   await contracts.quests.expirequest(seconduserQ1.fixed.hash, { authorization: `${seconduser}@active` })

//   await checkNodes(assert, variableDetails, {
//     nodes: await getQuests(seconduser),
//     label: 'status',
//     value: 'expired',
//     given: 'expired quest',
//     should: 'have the correct status'
//   }, 1)

// })

describe('Voted quests timeouts', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  const passProposal = async (proposalHash) => {
    await contracts.quests.favour(thirduser, proposalHash, 1, { authorization: `${thirduser}@active` })
    await sleep(300)
    await contracts.quests.favour(fourthuser, proposalHash, 1, { authorization: `${fourthuser}@active` })
    await sleep(300)
    await contracts.quests.favour(fifthuser, proposalHash, 1, { authorization: `${fifthuser}@active` })
    await sleep(300)
  }

  const contracts = await initContracts({ quests, accounts, token, settings, escrow, proposals })

  console.log('reset settings')
  await contracts.settings.reset({ authorization: `${settings}@active` })

  console.log('reset accounts')
  await contracts.accounts.reset({ authorization: `${accounts}@active` })

  console.log('reset proposals')
  await contracts.proposals.reset({ authorization: `${proposals}@active` })

  console.log('reset escrow')
  await contracts.escrow.reset({ authorization: `${escrow}@active` })

  console.log('reset quests')
  await contracts.quests.reset({ authorization: `${quests}@active` })

  console.log('join users')
  await contracts.accounts.adduser(firstuser, 'firstuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(seconduser, 'seconduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(thirduser, 'thirduser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fourthuser, 'fourthuser', 'individual', { authorization: `${accounts}@active` })
  await contracts.accounts.adduser(fifthuser, 'fifthuser', 'individual', { authorization: `${accounts}@active` })

  await contracts.accounts.testcitizen(firstuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(seconduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(thirduser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(fourthuser, { authorization: `${accounts}@active` })
  await contracts.accounts.testcitizen(fifthuser, { authorization: `${accounts}@active` })

  console.log('running on proposals::onperiod')
  await contracts.proposals.addvoice(firstuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(seconduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(thirduser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(fourthuser, 44, { authorization: `${proposals}@active` })
  await contracts.proposals.addvoice(fifthuser, 44, { authorization: `${proposals}@active` })

  console.log('add private quest')
  await contracts.quests.addquest(firstuser, '100.0000 SEEDS', 'Test quest 2', 'Test quest 2', campaignbank, 1, { authorization: `${firstuser}@active` })
  await contracts.quests.addquest(seconduser, '200.0000 SEEDS', 'Test quest 1', 'Test quest 1', campaignbank, 1, { authorization: `${seconduser}@active` })
  
  await sleep(400)

  const firstuserQuests = await getQuests(firstuser)
  const firstuserQuest = firstuserQuests[0]

  const seconduserQuests = await getQuests(seconduser)
  const seconduserQuest = seconduserQuests[0]

  console.log('add milestones')
  await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 1', 'milestone description', 2000, { authorization: `${firstuser}@active` })
  await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 2', 'milestone description', 2050, { authorization: `${firstuser}@active` })
  await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 3', 'milestone description', 2050, { authorization: `${firstuser}@active` })
  await contracts.quests.addmilestone(firstuserQuest.fixed.hash, 'Test milestone title 4', 'milestone description', 3900, { authorization: `${firstuser}@active` })

  await contracts.quests.addmilestone(seconduserQuest.fixed.hash, 'Test milestone title 1', 'milestone description', 10000, { authorization: `${seconduser}@active` })
  await contracts.quests.addmilestone(seconduserQuest.fixed.hash, 'Test milestone title 2', 'milestone description', 8000, { authorization: `${seconduser}@active` })

  console.log('delete milestone')
  const ms = await getMilestones(seconduserQuest.fixed.hash)
  await contracts.quests.delmilestone(ms[1].fixed.hash, { authorization: `${seconduser}@active` })

  console.log('propose quest')
  await contracts.quests.activate(seconduserQuest.fixed.hash, { authorization: `${seconduser}@active` })
  await contracts.quests.activate(firstuserQuest.fixed.hash, { authorization: `${firstuser}@active` })

  console.log('active proposals')
  await contracts.quests.onperiod('no', { authorization: `${quests}@active` })
  await sleep(5000)

  const proposals1 = await getProposals()

  console.log('vote for proposals')
  await passProposal(proposals1[0].fixed.hash)
  await passProposal(proposals1[1].fixed.hash)
  
  console.log('evaluate proposals')
  await contracts.quests.onperiod('yes', { authorization: `${quests}@active` })
  await sleep(2000)
  await contracts.quests.onperiod('yes', { authorization: `${quests}@active` })
  await sleep(2000)

  // console.log('apply for a quest')
  // await contracts.quests.apply(firstuserQuest.fixed.hash, thirduser, 'Applicant description', { authorization: `${thirduser}@active` })
  // await contracts.quests.apply(seconduserQuest.fixed.hash, fourthuser, 'Applicant description', { authorization: `${fourthuser}@active` })




















  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  // console.log('accept applicant')
  // const activeProposals1 = await getProposals('stage', 'active')
  // await passProposal(activeProposals1[0].fixed.hash)
  // await passProposal(activeProposals1[1].fixed.hash)

  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  
  // console.log('expire applicant')
  // await contracts.settings.configure('qst.exp.appl', 1, { authorization: `${settings}@active` })
  // await sleep(2000)

  // const firstuserQuestApplicants = await getApplicants(firstuserQuest.fixed.hash)

  // await contracts.quests.expireappl(firstuserQuestApplicants[0].fixed.hash, { authorization: `${firstuser}@active` })

  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  // const activeProposals2 = await getProposals('stage', 'active')
  // await passProposal(activeProposals2[0].fixed.hash)

  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  // await checkNodes(assert, variableDetails, {
  //   nodes: (await getApplicants(firstuserQuest.fixed.hash))[0],
  //   label: 'status',
  //   value: 'expired',
  //   given: 'expired applicant',
  //   should: 'have the correct status'
  // })


  // console.log('cancel applicant')
  // const seconduserQuestMaker = await getMaker(seconduserQuest.fixed.hash)
  // await contracts.quests.accptquest(seconduserQuest.fixed.hash, { authorization: `${seconduserQuestMaker.fixed.creator}@active` })

  // await sleep(1000)

  // await contracts.quests.cancelappl(seconduserQuestMaker.fixed.hash, { authorization: `${seconduser}@active` })

  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  // const activeProposals3 = await getProposals('stage', 'active')
  // await passProposal(activeProposals3[0].fixed.hash)

  // console.log('active proposals')
  // await contracts.quests.onperiod({ authorization: `${quests}@active` })
  // await sleep(5000)

  // await checkNodes(assert, variableDetails, {
  //   nodes: await getApplicants(seconduserQuest.fixed.hash),
  //   label: 'status',
  //   value: 'cancel',
  //   given: 'canceled applicant',
  //   should: 'have the correct status'
  // })

})


// describe('Private quests', async assert => {

//   if (!isLocal()) {
//     console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
//     return
//   }

//   const contracts = await initContracts({ quests, accounts, token, settings, escrow })

//   console.log('reset settings')
//   await contracts.settings.reset({ authorization: `${settings}@active` })

//   console.log('reset accounts')
//   await contracts.accounts.reset({ authorization: `${accounts}@active` })

//   console.log('reset escrow')
//   await contracts.escrow.reset({ authorization: `${escrow}@active` })

//   console.log('reset quests')
//   await contracts.quests.reset({ authorization: `${quests}@active` })


//   console.log('test transfer')
//   await contracts.quests.test1(4, { authorization: `${quests}@active` })

// })

