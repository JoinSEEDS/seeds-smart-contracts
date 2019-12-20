# Contract Names

See here: https://gitlab.com/seeds-project/seeds-contracts/issues/25

# Compiler Setup - Docker or native

Set up env file by copying the example to .env

```
cp .env.example .env
```

See here: https://gitlab.com/seeds-project/seeds-contracts/blob/master/.env.example

The COMPILER variable can either be docker or local - if you have eos-cpp installed on your local machine you can use local, if you want to use a docker container make sure docker is running and it'll do everything for you.

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
