const { Blockchain, nameToBigInt, symbolCodeToBigInt, addInlinePermission,
        expectToThrow } = require("@proton/vert");
const { Asset, TimePoint } = require("@greymass/eosio");
const { assert, expect } = require("chai");
const blockchain = new Blockchain()

// Tokensmaster test  TBD

