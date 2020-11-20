require('dotenv').config()
const fs = require(`fs`)
const path = require(`path`)
const Mustache = require('mustache')
const cheerio = require('cheerio')
const commonmark = require('commonmark')
const { Serialize, api } = require(`eosjs`)

const { exec } = require('child_process')
const { promisify } = require('util')

const { source } = require('./deploy')

const existsAsync = promisify(fs.exists)
const mkdirAsync = promisify(fs.mkdir)
const unlinkAsync = promisify(fs.unlink)
const readFileAsync = promisify(fs.readFile)
const execAsync = promisify(exec)

const docsgen = async (contract) => {
    try {
        const abiPath = path.join(__dirname, '../artifacts', contract.concat('.abi'))
        const outputPath = path.join(__dirname, '../docs', contract.concat('.html'))
        const templatePath = path.join(__dirname, '../docs/docstemplate.html')
        var abiFile = await readFileAsync(abiPath, "utf8")
        var templateFile = await readFileAsync(templatePath, "utf8")
        console.log(`generating docs for: ${contract}`)
        const $ = cheerio.load(templateFile)
        var template = $('#template').html();
        var rendered = Mustache.render(template, JSON.parse(abiFile));
        $('#target').html(rendered);
        var reader = new commonmark.Parser();
        var writer = new commonmark.HtmlRenderer();
        var parsed = reader.parse($('#target').html());
        var result = writer.render(parsed);
        $('#target').html(result);

        fs.writeFile(outputPath, $.html(), function (err) {
            if (err) throw err;
            console.log('Saved!');
          });
    } catch (err) {
        console.log("doc generation failed for " + contract + " error: " + err)
    }
}

module.exports = docsgen