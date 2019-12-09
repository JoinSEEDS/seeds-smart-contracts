const fs = require('fs')
const { names, getEOSWithEndpoint } = require('../helper')

const { accounts, invites } = names

const resultsArray = []

const testEndpoint = async (endpoint) => {
    var start = new Date().getTime();
    try {
        console.log("Testing "+endpoint)

        const eos = getEOSWithEndpoint(endpoint)
        const users = await eos.getTableRows({
            code: accounts,
            scope: accounts,
            table: 'users',
            json: true,
            limit: 1000
        })
        const { rows } = users
        var end = new Date().getTime();
        const time = end - start
        console.log("found "+rows.length + " users in "+time)

        let {actions} = await eos.getActions(invites, 0, 20)

        console.log("HISTORY LENGTH: "+actions.length)
        console.log("HISTORY "+JSON.stringify(actions, null, 2))

        resultsArray.push(
            {
                numResults: rows.length,
                time: time,
                endpoint: endpoint,
                actions: actions,
                error: ""
            }
        )
    
    } catch (error) {
        var end = new Date().getTime();
        const time = end - start

        console.log("Error in endpoint "+endpoint + " error: "+error)
        resultsArray.push(
            {
                numResults: -1,
                time: time,
                endpoint: endpoint,
                actions: [],
                error: "" + error
            }
        )
    }

}

const main = async () => {
  
    const endpoints = fs.readFileSync('endpoints.csv').toString().split("\n")

    for (let k = 0; k < endpoints.length; k++) {
        let endpoint = endpoints[k]
        endpoint = endpoint.replace(/^\s+|\s+$/g, '');
        endpoint = endpoint.replace("http://", "https://")
        if (endpoint == "" || endpoint.startsWith("#")) {
            continue
        }

        await testEndpoint(endpoint)

    }
    let sortedResults = resultsArray.sort(function (a, b) {
        if (a.numResults == b.numResults) {
            if (a.time < b.time) {
                return -1
            } else {
                return 1
            }
        }
        if (a.numResults > b.numResults) {
            return -1;
        } else {
            return 1;
        }
    });


    console.log("=============================================================================")
    console.log("========= RESULTS ")
    console.log("=============================================================================\n\n")

    let str = ""

    for (let ix = 0; ix < sortedResults.length; ix++) {
        let r = sortedResults[ix]
        let actionString = "history items: "+ fillString(r.actions.length + "", 5)
        console.log(fillString(r.endpoint, 40) + "results: "+ fillString(r.numResults + "", 10) +actionString+ "\ttime: "+ (r.time / 1000.0) + " seconds")
        str = str + r.endpoint + "," + r.numResults + "," +r.actions.length + "," + r.time + "\n"
        if (r.error != undefined && r.error != "") {
            let errStr = "error: "+r.error 
            //console.log(errStr.substr(0, 160))
        }
    }
    fs.writeFileSync('endpoints_ranking.csv', str)
    console.log('done')
}

const fillString = (str, toLength) => {
    var s = str
    while (s.length < toLength) {
        s = s + " "
    }
    return s
}

main()
