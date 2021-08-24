const fs = require('fs')
const { eos, names, initContracts } = require('./helper')

const { referendums } = names


const titles =  { 
"whatIsProposal": { 
  "rank": 0,
  "label": "What is the proposal?",
},
"tensionOrConflict": {
  "rank": 1,
  "label": "What inspired this proposal?",
},
"expectedResult": {
  "rank": 2,
  "label": "What do you expect will happen with this change?",
},
"downsides": {
  "rank": 3,
  "label": "Are there any downsides?",
  "placeholder": "Enter a number"
},
}


const main = async () => {

  console.log("BACKING UP props ")

  const contracts = await initContracts({ referendums })

  const props = await eos.getTableRows({
    code: referendums,
    scope: "active",
    table: 'referendums',
    reverse: false,
    json: true,
    limit: 100
  })

  fs.writeFileSync('refss_backup_.json', JSON.stringify(props, null, 2))

  for (var i=0; i<props.rows.length; i++) {
    var prop = props.rows[i]
    if (prop.description.indexOf("{") != 0) {
      console.log("not json "+prop.referendum_id)
      continue
    }
    var desc = JSON.parse(prop.description)

    var keys = Object.keys(desc)
    
    var output = ""
    
    console.log("\n\n   prop id: "+prop.referendum_id)
    console.log("=== Title: "+prop.title)
    console.log("====================================")

    keys.sort((k1, k2) => titles[k1].rank - titles[k2].rank)

    console.log("keys: "+JSON.stringify(keys))

    keys.forEach(k => {
      var content = desc[k]
      if (content != "") {
        var title = titles[k].label
        console.log("==============")
        console.log(title)
        console.log("==============")
        output = output + "* "+ title
        output = output + "\n\n"
        //console.log(JSON.stringify(content, null, 2))
        if (titles[k].rank == 0) {
          content = "Ratifying the SEEDS Constitution\n\n" + content
        }

        output = output + content
        if (content[content.length-1] != "\n") {
          output = output + "\n"
        }
        output = output + "\n\n"

      }

      
    })
    console.log("final text:\n"+output)



    //console.log('apply fix')
    //console.log("disabled!!! ")
    await contracts.referendums.fixdesc(prop.referendum_id, output, { authorization: `${referendums}@active` })
  

  }

    
  
}

main()
