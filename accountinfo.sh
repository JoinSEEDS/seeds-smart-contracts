echo "ACCOUNT"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds users

echo "VOICE"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  funds.seeds funds.seeds voice 

echo "REP"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds rep 

echo "REFERER"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds refs

echo "VOUCH"
cleos -u https://node.hypha.earth/ get table accts.seeds $1 vouches

echo "VOUCH TOTALS"
cleos -u https://node.hypha.earth/ get table --lower $1 --upper $1  accts.seeds accts.seeds vouchtotals

#cleos -u https://node.hypha.earth/ get table funds.seeds loveandlight voice --limit 300
