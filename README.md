# Contract Names

See here: https://gitlab.com/seeds-project/seeds-contracts/issues/25

# Setup

### Git
```
git submodule init
git submodule update
```


### Environment

The .env file contains the all-important keys for local, testnet, and potentially mainnet

It also contains a compiler setting - use either local compiler or Docker based compiler

Copy the example to .env

```
cp .env.example .env
```

### Compiler Setup in .env file

The COMPILER variable can either be docker or local - if you have eos-cpp installed on your local machine you can use local, if you want to use a docker container make sure docker is running and it'll do everything for you.

### Tools Setup

```
npm install
```

# Deploy Tools

Use the seeds.js script to 

### init all contracts and deploy them on local network

```
./scripts/seeds.js init
```

### update contract permissions

This command will update all permissions on all contracts

It will check if a permission is already set and only set permissions that
have been added or have been changed.

```
./scripts/seeds.js updatePermissions
```

### Compile, deploy, or test a contract

```
./scripts/seeds.js compile harvest => compiles seeds.harvest.cpp
```
```
./scripts/seeds.js deploy accounts => deploys accounts contract
```
```
./scripts/seeds.js test accounts => run unit tests on accounts contract
```
```
./scripts/seeds.js run accounts => compile, deploy, and run unit tests
```
### Specify more than one contract - 

Contract is a varadic parameter

```
./scripts/seeds.js run accounts onboarding organization
```

### Deploy on testnet
```
EOSIO_NETWORK=telosTestnet ./scripts/seeds.js deploy accounts
```
### Deploy on mainnet
```
EOSIO_NETWORK=telosMainnet ./scripts/seeds.js deploy accounts
```

### usage seeds.js 
```
./scripts/seeds.js <command> <contract name> [additional contract names...]
command = compile | deploy | test | run
```


### run a contract - compile, then deploy, then test 

```
example: 
./scripts/seeds.js run harvest => compiles seeds.harvest.cpp, deploys it, runs unit tests
```

### generate contract documentation

This command will generate html automatically based on the contract ABI files.

The <comment> tags inside the documents will be left untouched, even when they are regenerated.


This will generate docs only for the `accounts` contract.
```
./scripts/seeds.js docsgen accounts:
```

This will generate all contracts:
```
./scripts/seeds.js docsgen all
```

This will regenerate the index.html file:
```
./scripts/seeds.js docsgen index
```
