# Contract Names

See here: https://gitlab.com/seeds-project/seeds-contracts/issues/25

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
