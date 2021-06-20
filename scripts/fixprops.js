const fs = require('fs')
const { eos, names, initContracts } = require('./helper')

const { proposals, token, harvest } = names

const getBalanceFloat = async (user) => {
  const balance = await eos.getCurrencyBalance(names.token, user, 'SEEDS')
  let num = parseFloat(balance[0])
  if (num == NaN) {
    console.log("nil balance for "+user+": "+JSON.stringify(balance))
  }
  return parseFloat(balance[0])
}


const titles = {
  "cmp.funding": { 
"purpose": { 
  "rank": 0,
  "label": "What is the proposal?",
  "placeholder": "In as few words as possible, tell us what you’d like to change."
},
"howWillServe": {
  "rank": 2,
  "label": "How will this serve the regenerative movement?",
  "placeholder": "How does this campaign serve people and / or our planet?"
},
"usage": {
  "rank": 1,
  "label": "How will these Seeds be used? How do people participate?",
  "placeholder": "Get people excited to participate in your campaign"
},
"howManyPeople": {
  "rank": 3,
  "label": "How many people will benefit from this campaign?",
  "placeholder": "Enter a number"
},
"whyThisAmount": {
  "rank": 4,
  "label": "Why are you asking for this amount?",
  "placeholder": "A single SEED can grow a tree"
}
}, "alliance" :{
  "purpose": {
    "rank": 0,
    "label": "What's your purpose?",
    "placeholder": "Briefly describe your organization's purpose"
  },
  "howWillServe": {
    "rank": 2,
    "label": "How will this serve the regenerative movement?",
    "placeholder": "Tell us how your organization is regenerative"
  },
  "usage": {
    "rank": 1,
    "label": "What value are you offering the SEEDS community or the regenerative movement? Why should people support this Alliance",
    "placeholder": "Tell us what impact you have or plan to have with this support?"
  },
  "howManyPeople": {
    "rank": 3,
    "label": "What is the current reach of your regenerative impact?"
  },
  "whyThisAmount": {
    "rank": 4,
    "label": "Why are you asking for this amount?",
    "placeholder": "A single SEED can grow a tree"
  },
}
}


const main = async () => {

  console.log("BACKING UP props ")

  const contracts = await initContracts({proposals })

  const props = await eos.getTableRows({
    code: proposals,
    scope: proposals,
    table: 'props',
    reverse: false,
    lower_bound: 182,
    json: true,
    limit: 100
  })

  fs.writeFileSync('props_backup_.json', JSON.stringify(props, null, 2))

  var allKeys = []



  for (var i=0; i<props.rows.length; i++) {
    var prop = props.rows[i]
    if (prop.description.indexOf("{") != 0) {
      console.log("not json "+prop.id)
      continue
    }
    var desc = JSON.parse(prop.description)

    var keys = Object.keys(desc)
    
    var output = ""
    
    console.log("\n\n   prop id: "+prop.id)
    console.log("=== Title: "+prop.title)
    console.log("====================================")

    keys.sort((k1, k2) => titles[prop.campaign_type][k1].rank - titles[prop.campaign_type][k2].rank)

    console.log("keys: "+JSON.stringify(keys))

    keys.forEach(k => {
      if (allKeys.indexOf(k) == -1) { 
        allKeys.push(k)
      } 
      var content = desc[k]
      if (content != "") {
        var title = titles[prop.campaign_type][k].label
        // console.log("==============")
        // console.log(title)
        // console.log("==============")
        output = output + "* "+ title
        output = output + "\n\n"
        //console.log(JSON.stringify(content, null, 2))

        output = output + content
        if (content[content.length-1] != "\n") {
          output = output + "\n"
        }
        output = output + "\n\n"

      }

      
    })
    console.log("final text:\n"+output)

    console.log('apply fix')
    console.log("disabled!!! ")
    //await contracts.proposals.fixdesc(prop.id, output, { authorization: `${proposals}@active` })
  

  }

    
  
}

main()
