const { Blockchain, nameToBigInt, symbolCodeToBigInt, addInlinePermission } = require("@proton/vert");
const { Asset, TimePoint } = require("@greymass/eosio");
const { assert, expect } = require("chai");
const blockchain = new Blockchain()

// Load contract (use paths relative to the root of the project)
const rainbows = blockchain.createContract('rainbows', 'fyartifacts/rainbows')
const seeds = blockchain.createContract('token.seeds', 'fyartifacts/token.seeds')
const symSEEDS = Asset.SymbolCode.from('SEEDS')
const symTOKES = Asset.SymbolCode.from('TOKES')

var accounts
var rows
async function initSEEDS() {
    await seeds.actions.create(['master', '2000000.0000 SEEDS']).send('token.seeds@active')
    await seeds.actions.issue(['master', '1000000.0000 SEEDS', 'issue some']).send('master@active')
    rows = seeds.tables.stat(symbolCodeToBigInt(symSEEDS)).getTableRows()
    assert.deepEqual(rows, [ { supply: '1000000.0000 SEEDS', max_supply: '2000000.0000 SEEDS', issuer: 'master' } ] )
}
async function initTOKES(starttimeString) {
    await rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
                     starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
    await rainbows.actions.approve(['TOKES', false]).send('rainbows@active')
                     
    const cfg = rainbows.tables.configs(symbolCodeToBigInt(symTOKES)).getTableRows()
    assert.deepEqual(cfg, [ { withdrawal_mgr: 'issuer', withdraw_to: 'user3', freeze_mgr: 'issuer',
        redeem_locked_until: starttimeString, config_locked_until: starttimeString,
        transfers_frozen: false, approved: true, membership: '\x00', broker: '\x00',
        cred_limit: '\x00', positive_limit: '\x00' } ] )
    rows = rainbows.tables.stat(symbolCodeToBigInt(symTOKES)).getTableRows()
    assert.deepEqual(rows, [ { supply: '0.00 TOKES', max_supply: '1000000.00 TOKES',
        issuer: 'issuer'  } ] )
}
/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
    accounts = await blockchain.createAccounts('master', 'issuer', 'escrow', 'user2', 'user3', 'user4')
    await initSEEDS()
})

/* Tests */
describe('Rainbow', () => {
    it('create TOKES token', async () => {
        const starttime = new Date()
        starttime.setSeconds(0, 0)
        const starttimeString = starttime.toISOString().slice(0, -1);
        await initTOKES(starttimeString)
    });
    it('issue backed token', async () => {
        const starttime = new Date()
        starttime.setSeconds(0, 0)
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))
        const starttimeString = starttime.toISOString().slice(0, -1);
        await initTOKES(starttimeString)
        await seeds.actions.transfer(['master','issuer','100.0000 SEEDS','']).send('master@active')
        await rainbows.actions.setbacking(['5.00 TOKES', '2.0000 SEEDS', 'token.seeds', 'escrow', false, 100, '']).send('issuer@active')
        issuer_account = 
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'issuer').permissions )
        await rainbows.actions.issue([ '25.00 TOKES', 'issue some']).send('issuer@active')
        rows = seeds.tables.accounts([nameToBigInt('escrow')]).getTableRows()
        assert.deepEqual(rows, [ { balance: '10.0000 SEEDS'} ] )
    });
        
});
