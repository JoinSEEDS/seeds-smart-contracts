echo "ACCOUNT"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds users

echo "VOICE"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  funds.seeds funds.seeds voice 

echo "REP"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds rep 


#cleos -u https://node.hypha.earth/ get table funds.seeds loveandlight voice --limit 300
