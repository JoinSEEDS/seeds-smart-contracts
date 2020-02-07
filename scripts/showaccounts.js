const fs = require('fs')
const { eos, names } = require('./helper')
const program = require('commander')

const fillString = (str, toLength) => {
  var s = str
  while (s.length < toLength) {
      s = s + " "
  }
  return s
}

const printCritical = (name, list) => {
  console.log("==================================================================")
  if (list.length == 0) {
    console.log("Critical "+name+": None")
    return;
  }
  console.log("Critical "+name+": " + list.length)

  for (i=0; i<list.length; i++) {
    let str = list[i]
    console.log(str)
  }
  console.log("==================================================================")

}

const run = async (verbose, filename) => {

  console.log("LIST Seeds Accounts "+ (verbose ? "verbose" : "") + (filename ? " write to file: "+filename : ""))

  let headerLine = "contract,ram use (KB),cpu use,net use,ram %,cpu %,net %,ram available (KB), cpu staked TLOS, net staked TLOS"
  var lines = headerLine + "\n"

  if (verbose) {
    console.log(headerLine)
  } else {
    process.stdout.write("checking... ")
  }


  var limit = 80

  var flagged_net = []
  var flagged_cpu = []
  var flagged_ram = []

  for (const [key, value] of Object.entries(names)) {
    //console.log(key +": "+ JSON.stringify(value, null, 2));

    const info = await eos.getAccount(value)
    //console.log(value + ": "+JSON.stringify(info, null, 2))

    let ramused = info.ram_usage 
    let ram = info.total_resources.ram_bytes
    let cpu = info.total_resources.cpu_weight
    let net = info.total_resources.net_weight

    let ram_percent = parseInt(ramused*100/ram)

    let cpu_used = info.cpu_limit.used
    let cpu_available = info.cpu_limit.available
    let cpu_percent = parseInt((cpu_used / cpu_available)*100)


    //let cpu_percent_str = cpu_percent + "%"
    //let cpu_str = "CPU "+cpu_used + " / " +cpu_available+ "  " + parseInt((cpu_used / cpu_available)*100) + "%"

    let net_used = info.net_limit.used
    let net_available = info.net_limit.available
    let net_percent = parseInt((net_used / net_available)*100)

    //let net_percent_str = net_percent + "%"
    //let net_str = "NET: "+net_used + " / " +net_available+ "  " + parseInt((net_used / net_available)*100) + "%"

    let line = (value+","+
      parseInt(ramused/1000)+","+
      cpu_used+","+
      net_used+","+
      ram_percent+","+
      cpu_percent+","+
      net_percent+","+
      parseInt(ram/1000)+","+
      parseInt(cpu)+","+
      parseInt(net)
    )

    if (verbose) {
      console.log(line)
    } else {
      process.stdout.write(value + " .. ")
    }

    lines = lines + line + "\n"

    if (ram_percent > limit) {
      let str = "* " + value + " RAM: " + ram_percent
      console.log("critical RAM: "+str)
      flagged_ram.push(str)
    }
    if (cpu_percent > limit) {
      let str = "* " + value + " CPU: " + cpu_percent
      console.log("critical CPU: "+str)

      flagged_cpu.push(str)
    }
    if (net_percent > limit) {
      let str = "* " + value + " NET: " + net_percent
      console.log("critical NET: "+str)

      flagged_net.push(str)
    }
    
  }

  printCritical("RAM", flagged_ram)
  printCritical("CPU", flagged_cpu)
  printCritical("NET", flagged_net)

  if (filename) {
    console.log("writing to "+filename)
    fs.writeFileSync(filename, ""+lines);
  }



}

program
.option('-v, --verbose', 'output extra debugging')  
.option('-f, --file <filename>', 'write results to file')  
.description('Show all .seeds accounts resource usage')

program.parse(process.argv)

if (program.file && program.file.indexOf("-") == 0) {
  console.log("\nERROR: specify filename!\n")
  program.help();
} else {
  run(program.verbose, program.file)
}

