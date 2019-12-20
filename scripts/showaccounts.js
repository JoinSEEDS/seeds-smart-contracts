const fs = require('fs')
const { eos, names } = require('./helper')

const fillString = (str, toLength) => {
  var s = str
  while (s.length < toLength) {
      s = s + " "
  }
  return s
}

const main = async () => {

  console.log("LIST ACCOUNTS")

  console.log("contract,ram use (KB),cpu use,net use,ram %,cpu %,net %,ram available (KB), cpu staked TLOS, net staked TLOS")
  for (const [key, value] of Object.entries(names)) {
    //console.log(key +": "+ JSON.stringify(value, null, 2));

    const info = await eos.getAccount(value)
    //console.log(value + ": "+JSON.stringify(info, null, 2))

    let ramused = info.ram_usage 
    let ram = info.total_resources.ram_bytes
    let cpu = info.total_resources.cpu_weight
    let net = info.total_resources.net_weight

    let cpu_used = info.cpu_limit.used
    let cpu_available = info.cpu_limit.available

    let cpu_percent = parseInt((cpu_used / cpu_available)*100)
    let cpu_percent_str = cpu_percent + "%"
    let cpu_str = "CPU "+cpu_used + " / " +cpu_available+ "  " + parseInt((cpu_used / cpu_available)*100) + "%"

    let net_used = info.net_limit.used
    let net_available = info.net_limit.available
    let net_percent = parseInt((net_used / net_available)*100)
    let net_percent_str = net_percent + "%"
    let net_str = "NET: "+net_used + " / " +net_available+ "  " + parseInt((net_used / net_available)*100) + "%"

    console.log(value+","+
      parseInt(ramused/1000)+","+
      cpu_used+","+
      net_used+","+
      parseInt(ramused*100/ram)+","+
      cpu_percent+","+
      net_percent+","+
      parseInt(ram/1000)+","+
      parseInt(cpu)+","+
      parseInt(net)
    )

    // console.log(
    //   fillString(value, 12) + " " + 
    //   fillString("RAM: "+parseInt(ramused/1000) + " / "+parseInt(ram/1000) + " KB" , 24) + " " +
    //   fillString("CPU: "+cpu + " "+cpu_percent_str, 30) + 
    //   "NET: "+net +" "+net_percent_str)

    // console.log(fillString(" ", 38) + fillString(cpu_str, 30) + fillString(net_str, 30) )
      

  }
  // for (let i=0; i<names.length; i++) {
  //   let name = names[i]
  //   console.log("info on "+name)


  //   console.log(name + ": "+JSON.stringify(info, null, 2))

  // }

  //fs.writeFileSync('user_balances_'+token+'.json', JSON.stringify(account_balances, null, 2))



}

main()
