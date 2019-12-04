### Contract Names

## Owner Account

| Name | Purpose | Testnet Name
| ----- | ----- | ----- | 
| seedsharvest | main deploy account | s33dst3stn3t

## Contract Account names
| Contract Name | Purpose | Testnet Name
| ----- | ----- | ----- | 
| seedsaccntsx | user accounts contract | 
| seedshrvestx | harvest contract |
| seedsettingx | settings contract |
| seedsprpslsx | proposals contract |
| seedsinvitex | invites contract |
| seedsrfrndmx | referendums contract |
| seedshistorx | history contract |
| seedspolicyx | policy contract |
| seedstokennx | token contract |

## Active System Accounts
| Account Name | Purpose | 
| ------ | ------ | 
| seedsescrow1 | Escrow for invites | 

## Bank Accounts
| Account Name | Purpose |
| ------ | ------ | 
| giftingseeds | Community Airdrops & Earndrops 35% (Seeds) |
| mlstoneseeds | Hypha Milestone Completion 5% (Seeds) | 
| hyphasseedsx | Hypha Members 20% (Seeds) | 
| partnerseeds | Organization Partnerships 12% (Seeds) | 
| refrralseeds | Ambassador Commissions / Referrals 8% (Seeds) | 
| theseedsbank | Seeds Bank (D-Market Making) 20% (Seeds Locked in Bank Account) | 
| seedsbanksys | internal use account | 


## Testnet Test Accounts

These accounts exist only on local and testnet, they are never deployed on mainnet

| Account Name | Purpose |
| ----- | ----- |
| seedsuser444 | first user |
| seedsuser555 | second user |
| seedsuser333 | third user |


# Deploy Tools

Use the seeds.js script to 

 * init all contracts and deploy them on local network

```
example: ./scripts/seeds.js init
```

 * compile, deploy, or test a contract

```
example: ./scripts/seeds.js compile harvest => compiles seeds.harvest.cpp
```

```
spec: ./scripts/seeds.js [command] [contract name]
command = compile | deploy | test | run
```


 *  run a contract - which means to compile, then deploy, then test 

```
example: ./scripts/seeds.js run harvest => compiles seeds.harvest.cpp, deploys it, runs unit tests
```
