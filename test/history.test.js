const { describe } = require("riteway")
const { eos, encodeName, getBalance, names, getTableRows } = require("../scripts/helper")
const { equals } = require("ramda")

const { harvest, firstuser, history } = names

describe("make a history entry", async (assert) => {

    const history_contract = await eos.contract(history)

    console.log('history reset')
    await history_contract.reset(firstuser, { authorization: `${history}@active` })
    

    console.log('history make entry')
    await history_contract.historyentry(firstuser, "tracktest", 77,  "vasily", { authorization: `${history}@active` })

    console.log('check that history table has the entry')
    
    const { rows } = await getTableRows({
        code: history,
        scope: firstuser,
        table: "history",
        json: true
    })
    let timestamp = rows[0].timestamp

    let rowWithoutTimestamp = rows[0]
    delete rowWithoutTimestamp.timestamp
    assert({ 
        given: "action was tracked",
        should: "have table entry",
        actual: rowWithoutTimestamp,
        expected: {
            history_id: 0,
            account: firstuser,
            action: "tracktest",
            amount: 77,
            meta: "vasily",
        }
    })

    assert({ 
        given: "action was tracked",
        should: "timestamp is kinda close",
        actual: Math.round(timestamp/10),
        expected: Math.round(new Date()/10000),
    })


})