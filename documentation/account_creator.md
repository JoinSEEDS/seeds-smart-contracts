### Hypha Account Creator

Purpose: To create accounts on behalf of an oracle that knows whether or not we can create an acocunt for a user.


### Overview

The account creator contract (say, join.hypha) has a "create" function that takes an account name and key

It creates this account

It can be called by another contract, the oracle. Oracle contract can be configured using the "setconfig" action on the account creator contract

### Deployment

Compile contract and ABI

```./scripts/seeds.js run joinhypha```

Deploy using cleos

```cleos set contract join.hypha artifacts joinhypha.wasm joinhypha.abi```

Set permissions (fill in your existing public key for join.hypha)

```
cleos push action eosio updateauth '{
    "account": "join.hypha",
    "permission": "active",
    "parent": "owner",
    "auth": {
        "keys": [ {
            "key": "EOS5Bif4oEEMjKBi4gNsNcrB7Q8yQS7aYuxkgydCgxL3CVGTUBSA2",
            "weight": 1 }
        ],
        "threshold": 1,
        "accounts": [
            {
                "permission": {
                    "actor": "join.hypha",
                    "permission": "eosio.code"
                },
                "weight": 1
            }
        ],
        "waits": []
    }
}' -p join.hypha@owner
```

Configure the oracle account (here, hyphaoracle1)

```
cleos push action join.hypha setconfig '{
    "account_creator_contract": "hyphaoracle1",
    "account_creator_oracle": "hyphaoracle1"
    }
}' -p join.hypha@active
```