const { Blockchain, nameToBigInt, symbolCodeToBigInt, addInlinePermission,
        expectToThrow } = require("@proton/vert");
const { Asset, TimePoint } = require("@greymass/eosio");
const { assert, expect } = require("chai");
const blockchain = new Blockchain()

// Load contract (use paths relative to the root of the project)
const rainbows = blockchain.createContract('rainbows', 'fyartifacts/rainbows')
const seeds = blockchain.createContract('token.seeds', 'fyartifacts/token.seeds')
const symSEEDS = Asset.SymbolCode.from('SEEDS')
const symTOKES = Asset.SymbolCode.from('TOKES')
const symCREDS = Asset.SymbolCode.from('CREDS')
const symPROPS = Asset.SymbolCode.from('PROPS')

var accounts
var rows
var starttime
var starttimeString
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
    accounts = await blockchain.createAccounts('master', 'issuer', 'escrow', 'user2',
       'user3', 'user4', 'user5')
    starttime = new Date()
    starttime.setSeconds(0, 0)
    blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()))
    starttimeString = starttime.toISOString().slice(0, -1);

    await initSEEDS()
})

/* Tests */
describe('Rainbow', () => {
    it('did create TOKES token', async () => {
        await initTOKES(starttimeString)
    });
    it('did issue, transfer, withdraw, redeem fully backed token', async () => {
        await initTOKES(starttimeString)
        await seeds.actions.transfer(['master','issuer','1000.0000 SEEDS','']).send('master@active')
        await expectToThrow( 
            rainbows.actions.setbacking(['5.00 TOKES', '2.0000 SEEDS', 'token.seeds', 'escrow', false, 100, '']).send('issuer@active'),
            'eosio_assert: token reconfiguration is locked' )
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))  
        console.log('set backing')      
        await rainbows.actions.setbacking(['5.00 TOKES', '2.0000 SEEDS', 'token.seeds', 'escrow', false, 100, '']).send('issuer@active')        
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'issuer').permissions )
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'escrow').permissions )
        console.log('issue')
        await rainbows.actions.issue([ '500.00 TOKES', 'issue some']).send('issuer@active')
        rows = seeds.tables.accounts([nameToBigInt('escrow')]).getTableRows()
        assert.deepEqual(rows, [ { balance: '200.0000 SEEDS'} ] )
        await rainbows.actions.open(['user3', symTOKES, 'issuer']).send('issuer@active') // withdraw_to
        await rainbows.actions.open(['user4', symTOKES, 'issuer']).send('issuer@active')
        console.log('transfer')
        await rainbows.actions.transfer(['issuer','user4','100.00 TOKES', '']).send('issuer@active')
        await expectToThrow(
            rainbows.actions.transfer(['user4', 'issuer', '80.00 TOKES', '']).send('issuer@active'),
            'missing required authority user4' )
        console.log('withdraw')
        await rainbows.actions.transfer(['user4', 'user3', '80.00 TOKES', '']).send('issuer@active') // user3 = withdraw_to 
        balances = [ seeds.tables.accounts([nameToBigInt('escrow')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance: '200.0000 SEEDS'} ], [ { balance: '20.00 TOKES'} ] ] )
        console.log('redeem')
        await expectToThrow(
            rainbows.actions.retire(['user4', '30.00 TOKES', true, 'redeemed by user']).send('user4@active'),
            'eosio_assert: overdrawn balance' )
        await rainbows.actions.retire(['user4', '20.00 TOKES', true, 'redeemed by user']).send('user4@active')
        await rainbows.actions.retire(['user3', '80.00 TOKES', true, 'redeemed by user']).send('user3@active')
        await rainbows.actions.retire(['issuer', '400.00 TOKES', true, 'redeemed by isuer']).send('issuer@active')
        await seeds.actions.transfer(['user4', 'issuer', '8.0000 SEEDS', '']).send('user4@active')
        await seeds.actions.transfer(['user3', 'issuer', '32.0000 SEEDS', '']).send('user3@active')
        rows = seeds.tables.accounts([nameToBigInt('issuer')]).getTableRows()        
        assert.deepEqual(rows, [ { balance: '1000.0000 SEEDS'} ] )
        console.log('delete backing')
        await rainbows.actions.deletebacking([0, symTOKES, '']).send('issuer@active')
        rows = rainbows.tables.backings([ symbolCodeToBigInt(symTOKES) ]).getTableRows()
        assert.deepEqual(rows, [ ] )
    });
    it('did mutual credit token', async () => {
        console.log('create CRED and TOKES tokens')
        await rainbows.actions.create(['issuer', '1000000.00 CREDS', 'issuer', 'user3', 'issuer',
             starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await expectToThrow(
            rainbows.actions.issue(['1000000.00 CREDS', '']).send('issuer@active'),
            'eosio_assert: cannot issue until token is approved' )
        await rainbows.actions.approve(['CREDS', false]).send('rainbows@active')
        await rainbows.actions.issue(['1000000.00 CREDS', '']).send('issuer@active')
        await rainbows.actions.freeze(['CREDS', true, '']).send('issuer@active')
        await rainbows.actions.open(['user4', 'CREDS', 'issuer']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user4', '50.00 CREDS', '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user5', '100.00 CREDS', '']).send('issuer@active')
        console.log('transfer into credit')
        await expectToThrow(
            rainbows.actions.create(['issuer', '100.000 TOKES', 'issuer', 'user3', 'issuer',
            starttimeString, starttimeString, '', '', 'CREDS', '']).send('issuer@active'),
            'eosio_assert_message: CREDS token precision must be 3' )
        await rainbows.actions.freeze(['CREDS', false, '']).send('issuer@active')
        await expectToThrow(
            rainbows.actions.create(['issuer', '100.00 TOKES', 'issuer', 'user3', 'issuer',
            starttimeString, starttimeString, '', '', 'CREDS', '']).send('issuer@active'),
            'eosio_assert_message: CREDS token must be frozen' )
        await rainbows.actions.freeze(['CREDS', true, '']).send('issuer@active')
        await rainbows.actions.create(['issuer', '100.00 TOKES', 'issuer', 'user3', 'issuer',
            starttimeString, starttimeString, '', '', 'CREDS', '']).send('issuer@active')
        await expectToThrow(
            rainbows.actions.transfer(['user4', 'issuer', '50.00 TOKES', '']).send('user4@active'),
            'eosio_assert: token has not been approved' )
        await rainbows.actions.approve(['TOKES', false]).send('rainbows@active')
        console.log('check overdraw')
        await expectToThrow(
            rainbows.actions.transfer(['user4', 'issuer', '60.00 TOKES', '']).send('user4@active'),
            'eosio_assert: overdrawn balance' )
        await rainbows.actions.transfer(['user4', 'issuer', '50.00 TOKES', '']).send('user4@active')
        await rainbows.actions.transfer(['user5', 'issuer', '50.00 TOKES', '']).send('user5@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user5')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance: '50.00 CREDS' }, { balance: '-50.00 TOKES'} ],
             [ { balance: '100.00 CREDS' },  { balance: '-50.00 TOKES'} ] ] )
        rows = rainbows.tables.stat(symbolCodeToBigInt(symTOKES)).getTableRows()
        assert.deepEqual(rows, [ { supply: '100.00 TOKES', max_supply: '100.00 TOKES', issuer: 'issuer' } ] )
        console.log('check supply limit')
        await expectToThrow(
            rainbows.actions.transfer(['user5', 'issuer', '1.00 TOKES', '']).send('user5@active'),
            'eosio_assert: new credit exceeds available supply' )
        await rainbows.actions.transfer(['user5', 'user4', '20.00 TOKES', ''] ).send('user5@active')
        await rainbows.actions.transfer(['issuer', 'user4', '20.00 TOKES', ''] ).send('issuer@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user5')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance: '50.00 CREDS' }, { balance: '-10.00 TOKES'} ],
             [ { balance: '100.00 CREDS' },  { balance: '-70.00 TOKES'} ] ] )
        console.log('forced withdraw CREDS')
        await rainbows.actions.transfer(['user4', 'user3', '50.00 CREDS', ''] ).send('issuer@active')
    });   
    it('did proportionally backed token', async () => {
        console.log('create PROPS token')
        await rainbows.actions.create(['issuer', '1000000.0000 PROPS', 'issuer', 'user3', 'issuer',
                     starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await rainbows.actions.approve(['PROPS', false]).send('rainbows@active')
        await seeds.actions.transfer(['master', 'user4', '100000.0000 SEEDS', '']).send('master@active')
        await seeds.actions.transfer(['master', 'issuer', '2500.0000 SEEDS', '']).send('master@active')
        console.log('set backing')
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))  
        await expectToThrow(
            rainbows.actions.setbacking(['1.00 PROPS', '2.0000 SEEDS', 'token.seeds', 'escrow', true, 100, '']).send('issuer@active'),
            'eosio_assert: mismatched token_bucket precision' )
        await expectToThrow(
            rainbows.actions.setbacking(['1.0000 PROPS', '2.00 SEEDS', 'token.seeds', 'escrow', true, 100, '']).send('issuer@active'),
            'eosio_assert: mismatched backing token precision' )
        await expectToThrow(
            rainbows.actions.setbacking(['1.0000 PROPS', '2.0000 PROPS', 'rainbows', 'escrow', true, 100, '']).send('issuer@active'),
            'eosio_assert: cannot back with own token' )
        await rainbows.actions.setbacking(['1.0000 PROPS', '2.0000 SEEDS', 'token.seeds', 'escrow', true, 100, '']).send('issuer@active')
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'issuer').permissions )
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'escrow').permissions )
        console.log('issue PROPS')
        await rainbows.actions.issue(['500.0000 PROPS', '']).send('issuer@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('issuer')]).getTableRows(),
                     seeds.tables.accounts([nameToBigInt('issuer')]).getTableRows(),
                     seeds.tables.accounts([nameToBigInt('escrow')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'500.0000 PROPS' } ],
             [ { balance:'1500.0000 SEEDS' } ], [ { balance:'1000.0000 SEEDS' } ] ] )
        await rainbows.actions.transfer(['issuer', 'user4', '100.0000 PROPS', '']).send('issuer@active')
        console.log('redeem, before & after increasing backing in escrow account')
        await rainbows.actions.retire(['user4', '20.0000 PROPS', true, 'redeemed by user']).send('user4@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     seeds.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'80.0000 PROPS' } ], [ { balance:'100040.0000 SEEDS' } ] ] )
        await seeds.actions.transfer(['issuer', 'escrow', '480.0000 SEEDS', '+50% escrow']).send('issuer@active')
        await rainbows.actions.retire(['user4', '20.0000 PROPS', true, 'more redeemed by user']).send('user4@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     seeds.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'60.0000 PROPS' } ], [ { balance:'100100.0000 SEEDS' } ] ] )
    });
    it('did membership', async () => {
        console.log('create MEMBERS and TOKES tokens')
        await rainbows.actions.create(['issuer', '1000.00 MEMBERS', 'issuer', 'issuer', 'issuer',
             starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await rainbows.actions.approve(['MEMBERS', false]).send('rainbows@active')
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))  
        await expectToThrow(
            rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
                starttimeString, starttimeString, 'MEMBERS', '', '', '']).send('issuer@active'),
                'eosio_assert_message: MEMBERS token precision must be 0' )
        await rainbows.actions.create(['issuer', '1000 MEMBERS', 'issuer', 'issuer', 'issuer',
             starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await rainbows.actions.approve(['MEMBERS', false]).send('rainbows@active')
        await expectToThrow(
            rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
                starttimeString, starttimeString, 'MEMBERS', '', '', '']).send('issuer@active'),
                'eosio_assert_message: MEMBERS token must be frozen' )
        await expectToThrow(
            rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
                starttimeString, starttimeString, 'BADNAME', '', '', '']).send('issuer@active'),
                'eosio_assert: BADNAME token does not exist' )
        await rainbows.actions.create(['issuer', '1000 MEMBERS', 'issuer', 'issuer', 'issuer',
             starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await rainbows.actions.approve(['MEMBERS', false]).send('rainbows@active')
        await rainbows.actions.freeze(['MEMBERS', true, '']).send('issuer@active')
        await rainbows.actions.issue(['100 MEMBERS', ''] ).send('issuer@active')
        await rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
            starttimeString, starttimeString, 'MEMBERS', '', '', '']).send('issuer@active')
        await rainbows.actions.approve(['TOKES', false]).send('rainbows@active')
        await rainbows.actions.issue(['1000.00 TOKES','']).send('issuer@active')
        console.log('send tokens')
        await expectToThrow(
            rainbows.actions.transfer(['issuer', 'user4', '100.00 TOKES', '']).send('issuer@active'),
            'eosio_assert: to account must have membership' )
        await rainbows.actions.transfer(['issuer', 'user4', '1 MEMBERS', '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user5', '1 MEMBERS', '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user4', '100.00 TOKES', '']).send('issuer@active')
        await expectToThrow(
            rainbows.actions.transfer(['user4', 'user5', '100.00 TOKES', '']).send('user4@active'),
            'eosio_assert: cannot transfer visitor to visitor' )
        await rainbows.actions.transfer(['issuer', 'user5', '1 MEMBERS', '']).send('issuer@active')
        await rainbows.actions.transfer(['user4', 'user5', '100.00 TOKES', '']).send('user4@active')
        await rainbows.actions.transfer(['user5', 'user4', '40.00 TOKES', '']).send('user5@active')
        console.log('revoke membership')
        await rainbows.actions.transfer(['user4', 'issuer', '1 MEMBERS', 'revoke']).send('issuer@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user5')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance: '40.00 TOKES' }, { balance: '0 MEMBERS' } ],
                                     [ { balance: '60.00 TOKES' }, { balance: '2 MEMBERS' } ] ] )
    });
    it('did fractional backing', async () => {
        console.log('create FRACS token')
        await rainbows.actions.create(['issuer', '1000000.0000 FRACS', 'issuer', 'user3', 'issuer',
                     starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        await rainbows.actions.approve(['FRACS', false]).send('rainbows@active')
        await seeds.actions.transfer(['master', 'user4', '100000.0000 SEEDS', '']).send('master@active')
        await seeds.actions.transfer(['master', 'issuer', '2500.0000 SEEDS', '']).send('master@active')
        console.log('set backing')
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))  
        await rainbows.actions.setbacking(['1.0000 FRACS', '2.0000 SEEDS', 'token.seeds', 'escrow',
            false, 30, '']).send('issuer@active')
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'issuer').permissions )
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'escrow').permissions )
        console.log('issue FRACS')
        await rainbows.actions.issue(['500.0000 FRACS', '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user4', '100.0000 FRACS', '']).send('issuer@active')
        console.log('reduce escrow')
        await seeds.actions.transfer(['escrow', 'issuer', '500.0000 SEEDS', '']).send('escrow@active')
        console.log('redeem')
        await rainbows.actions.retire(['user4', '20.0000 FRACS', true, 'redeemed by user']).send('user4@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows(),
                     seeds.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'80.0000 FRACS' } ], [ { balance:'100040.0000 SEEDS' } ] ] )
        console.log('underfunded escrow')
        await seeds.actions.transfer(['escrow', 'issuer', '145.0000 SEEDS', '']).send('escrow@active')
        await expectToThrow(
            rainbows.actions.retire(['user4', '20.0000 FRACS', true, 'redeemed by user']).send('user4@active'),
            "eosio_assert_message: can't redeem, escrow underfunded in SEEDS (30% reserve)" )
    });
    it('did garner/demurrage', async () => {
        await initTOKES(starttimeString)
        await rainbows.actions.issue(['10000.00 TOKES', '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer','user4', '1000.00 TOKES', '']).send('issuer@active')
        console.log('garner 1% = 10000ppm')
        // must have rainbows@eosio.code permission on withdraw_mgr accout
        addInlinePermission( 'rainbows', accounts.find( (acct) => acct.name == 'issuer').permissions )
        await rainbows.actions.garner(['user4', 'user3', symTOKES, 10000, '']).send('issuer@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user3')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'10.00 TOKES' } ], [ { balance:'990.00 TOKES' } ] ] )
        console.log('confirm no garner to credit balance')
        await rainbows.actions.create(['issuer', '1000000.00 CREDS', 'issuer', 'user3', 'issuer',
             starttimeString, starttimeString, '', '', '', '', ]).send('issuer@active')
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()+1000))  
        await rainbows.actions.approve(['CREDS', false]).send('rainbows@active')
        await rainbows.actions.issue(['1000000.00 CREDS', '']).send('issuer@active')
        await rainbows.actions.freeze(['CREDS', true, '']).send('issuer@active')
        await rainbows.actions.transfer(['issuer', 'user4', '100.00 CREDS', '']).send('issuer@active')
        await rainbows.actions.create(['issuer', '1000000.00 TOKES', 'issuer', 'user3', 'issuer',
            starttimeString, starttimeString, '', '', 'CREDS', '']).send('issuer@active')
        await rainbows.actions.transfer(['user4', 'issuer', '1090.00 TOKES', '']).send('user4@active')
        await rainbows.actions.garner(['user4', 'user3', symTOKES, 10000, '']).send('issuer@active')
        balances = [ rainbows.tables.accounts([nameToBigInt('user3')]).getTableRows(),
                     rainbows.tables.accounts([nameToBigInt('user4')]).getTableRows() ]
        assert.deepEqual(balances, [ [ { balance:'10.00 TOKES' } ], [ { balance:'100.00 CREDS' }, { balance:'-100.00 TOKES' } ] ] )

        
    });

});
