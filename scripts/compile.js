require('dotenv').config()
const { exec } = require('child_process')
const { promisify } = require('util')
const fse = require('fs-extra')
var fs = require('fs');
var dir = './tmp';

const existsAsync = promisify(fs.exists)
const mkdirAsync = promisify(fs.mkdir)
const unlinkAsync = promisify(fs.unlink)
const execAsync = promisify(exec)

const command = ({ contract, source, include, dir, contractSourceName }) => {
    const volume = dir
    let cmd = ""
    let inc = include == "" ? "./include" : include

    contractSourceName = contractSourceName ?? contract
    
    if (process.env.COMPILER === 'local') {
      cmd = "eosio-cpp -abigen -I "+ inc +" -contract " + contractSourceName + " -o ./artifacts/"+contract+".wasm "+source;
    } else {
      cmd = `docker run --rm --name eosio.cdt_v1.7.0-rc1 --volume ${volume}:/project -w /project eostudio/eosio.cdt:v1.7.0-rc1 /bin/bash -c "echo 'starting';eosio-cpp -abigen -I ${inc} -contract ${contract} -o ./artifacts/${contract}.wasm ${source}"`
    }
    console.log("compiler command: " + cmd);
    return cmd
}

const compile = async ({ contract, source, include = "", contractSourceName }) => {
  // make sure source exists

  const contractFound = await existsAsync(source)
  if (!contractFound) {
    throw new Error('Contract not found: '+contract+' No source file: '+source);
  }

  const dir = process.cwd() + "/"
  // check directory
  // if (!dir.endsWith("seeds-smart-contracts/")) {
  //   throw new Error("You have to run from seeds-smart-contracts directory - comment out this line if installed in a different named folder ;)")
  // }
  const artifacts = dir + "artifacts"

  // make sure artifacts exists
  const artifactsFound = await existsAsync(artifacts)
  if (!artifactsFound){
    console.log("creating artifacts directory...")
    await mkdirAsync(artifacts)
  }

  // clean build folder
  await deleteIfExists(artifacts+"/"+contract+".wasm")
  await deleteIfExists(artifacts+"/"+contract+".abi")

  // copy document-graph submodule to the project's paths
  const docGraphInclude = dir + 'include/document_graph'
  const docGraphSrc = dir + 'src/document_graph'

  const docGraphIncludeFound = await existsAsync(docGraphInclude)
  const docGraphSrcFound = await existsAsync(docGraphSrc)

  if (!docGraphIncludeFound) {
    fse.copySync(dir + 'document-graph/include/document_graph', docGraphInclude, { overwrite: true }, (err) => {
      if (err) {
        throw new Error(''+err)
      } else {
        console.log("document graph submodule include prepared")
      }
    })
  }

  if (!docGraphSrcFound) {
    fse.copySync(dir + 'document-graph/src/document_graph', docGraphSrc, { overwrite: true }, (err) => {
      if (err) {
        throw new Error(''+err)
      } else {
        console.log("document graph submodule src prepared")
      }
    })
  }

  // run compile
  const execCommand = command({ contract, source, include, dir, contractSourceName })
  await execAsync(execCommand)
}

const deleteIfExists = async (file) => {
  const fileExists = await existsAsync(file)
  if (fileExists) {
    try {
      await unlinkAsync(file)
      //console.log("deleted existing ", file)
    } catch(err) {
      console.error("delete file error: "+err)
    }
  }
}

module.exports = compile
