# Contract Names

See here: https://gitlab.com/seeds-project/seeds-contracts/issues/25

# Setup

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

 * init all contracts and deploy them on local network

```
./scripts/seeds.js init
```

 * compile, deploy, or test a contract

```
./scripts/seeds.js compile harvest => compiles seeds.harvest.cpp
```

### usage seeds.js 
```
./scripts/seeds.js [command] [contract name]
command = compile | deploy | test | run
```


 *  run a contract - which means to compile, then deploy, then test 

```
example: 
./scripts/seeds.js run harvest => compiles seeds.harvest.cpp, deploys it, runs unit tests
```
