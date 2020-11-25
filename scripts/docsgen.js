require('dotenv').config()
const fs = require(`fs`)
const path = require(`path`)
const Handlebars = require("handlebars");
const cheerio = require('cheerio')
const commonmark = require('commonmark')
const { Serialize, api } = require(`eosjs`)

const { exec } = require('child_process')
const { promisify } = require('util')

const { source } = require('./deploy')

const readFileAsync = promisify(fs.readFile)
const appendFileAsync = promisify(fs.appendFile)

const docsgen = async (contract) => {
    try {
        var abiContents;
        var files;
        var templatePath;
        if (contract === "index") {
            const directoryPath = path.join(__dirname, '../docs');
            files = fs.readdirSync(directoryPath);
            //remove unwanted from the list
            files.splice(files.indexOf("index.html"), 1);
            files.splice(files.indexOf("docstemplate.html"), 1);
            abiContents = {"contracts": files.map(function(file) {
                return {"contract": "seeds."+file.substring(0, file.length-5),
                        "name": file.substring(0, file.length-5),
                        "file": file};
            })};
            templatePath = path.join(__dirname, '../docs/index.html')
        } else {
            const abiPath = path.join(__dirname, '../artifacts', contract.concat('.abi'))
            var abiFile = await readFileAsync(abiPath, "utf8")
            abiContents = JSON.parse(abiFile);
            templatePath = path.join(__dirname, '../docs/docstemplate.html')
        }

        const outputPath = path.join(__dirname, '../docs', contract.concat('.html'))
        abiContents["file"] = contract;

        // Imports comments from previous genrated page
        if (fs.existsSync(outputPath)) {
            var outFile = await readFileAsync(outputPath, "utf8")
            const $ = cheerio.load(outFile)
            $('#target comment').each(function(i, elem) {
                if ($(this).attr("type") == "main") {
                    abiContents["comment"] = $(this).text();
                } else {
                    const desired_type = $(this).attr("type")+"s"; 
                    const desired_name = $(this).attr("name");
                    var old = abiContents[desired_type].find(function (e) { 
                        return e["name"]==desired_name;
                    });
                    if (old) {
                        var pos = abiContents[desired_type].indexOf(old)
                        abiContents[desired_type][pos]["comment"] = $(this).text();    
                    }        
                }
            });
        }
        var templateFile = await readFileAsync(templatePath, "utf8")
        console.log(`generating docs for: ${contract}`)
        const $ = cheerio.load(templateFile)
        var templateCode = $('#template').html();
        const template = Handlebars.compile(templateCode);
        var rendered = template(abiContents);
        $('#target').html(rendered);
        var reader = new commonmark.Parser();
        var writer = new commonmark.HtmlRenderer();
        var parsed = reader.parse($('#target').html());
        var result = writer.render(parsed);
        $('#target').html(result);

        $('#target comment').each(function(i, elem) {
            if ($(this).attr("type") == "main") {
                $(this).text(abiContents["comment"]);
            } else {
                const desired_type = $(this).attr("type")+"s"; 
                const desired_name = $(this).attr("name");
                var old = abiContents[desired_type].find(function (e) { 
                    return e["name"]==desired_name;
                });
                if (old) {
                    var pos = abiContents[desired_type].indexOf(old)
                    $(this).text(abiContents[desired_type][pos]["comment"]);    
                }
            }
        });

        fs.writeFile(outputPath, $.html(), function (err) {
            if (err) throw err;
            console.log('Saved!');
          });
    } catch (err) {
        console.log("doc generation failed for " + contract + " error: " + err)
    }
}

module.exports = docsgen