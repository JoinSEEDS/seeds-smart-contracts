
cleos push action eosio updateauth '{
    "account": "escrow",
    "permission": "active",
    "parent": "owner",
    "auth": {
        "keys": [
            {
                "key": "EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ",
                "weight": 1
            }
        ],
        "threshold": 1,
        "accounts": [
            {
                "permission": {
                    "actor": "escrow",
                    "permission": "eosio.code"
                },
                "weight": 1
            }
        ],
        "waits": []
    }
}' -p escrow@owner

cleos create account eosio escrow EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ
cleos create account eosio sponsor EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ
cleos create account eosio user1 EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ
cleos create account eosio user2 EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ
cleos create account eosio user3 EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ
cleos create account eosio token EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ EOS5tEdJd32ANvoxSecRnY5ucr1jbzaVN2rQZegj6NxsevGU8JoaJ

cleos set contract token artifacts/token
cleos set contract escrow artifacts/escrow

cleos push action token create '["token", "1000.0000 SEEDS"]' -p token
cleos push action token issue '["token", "100.0000 SEEDS", "memo"]' -p token
cleos push action token transfer '["token", "sponsor", "100.0000 SEEDS", "memo"]' -p token
cleos push action token transfer '["sponsor", "escrow", "100.0000 SEEDS", "memo"]' -p sponsor

cleos push action escrow lock '["event", "sponsor", "user1", "10.0000 SEEDS", "golive2", "sponsor", "2020-01-24T14:30:58.000", "Initial event-based lock for go live- first genesis"]' -p sponsor
cleos push action escrow lock '["time", "sponsor", "user2", "10.0000 SEEDS", "na", "sponsor", "2020-01-27T00:38:08.000", "Initial event-based lock for go live- first genesis"]' -p sponsor
cleos push action escrow lock '["time", "sponsor", "user2", "10.0000 SEEDS", "na", "sponsor", "2020-01-27T00:38:08.000", "Initial event-based lock for go live- first genesis"]' -p sponsor
cleos push action escrow lock '["time", "sponsor", "user2", "10.0000 SEEDS", "na", "sponsor", "2020-01-27T00:38:08.000", "Initial event-based lock for go live- first genesis"]' -p sponsor
cleos push action escrow lock '["time", "sponsor", "user2", "10.0000 SEEDS", "na", "sponsor", "2020-01-27T00:38:08.000", "Initial event-based lock for go live- first genesis"]' -p sponsor

cleos push action escrow trigger '["sponsor", "golive2", "Yay"]' -p sponsor
cleos push action escrow claim '["user1"]' -p user1
cleos push action escrow claim '["user2"]' -p user2

cleos push action escrow withdraw '["sponsor", "10.0000 SEEDS"]' -p sponsor

cleos get table escrow escrow sponsors
cleos get table escrow escrow locks

cleos get table escrow sponsor events
