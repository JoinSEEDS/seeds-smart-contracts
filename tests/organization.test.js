require('dotenv').config()

const assert = require('assert');
const eoslime = require('eoslime').init();

const wasm_path = './artifacts/NAME.wasm';
const abi_path = './artifacts/NAME.abi';

let privateKey = process.env.LOCAL_PRIVATE_KEY;


let accounts = eoslime.Account.load('accts.seeds', privateKey, 'active');
let config = eoslime.Account.load('settgs.seeds', privateKey, 'active');
let organization = eoslime.Account.load('orgtns.seeds', privateKey, 'active');
let token = eoslime.Account.load('token.seeds', privateKey, 'active');
let firstuser = eoslime.Account.load('seedsuseraaa', privateKey, 'active');
let seconduser = eoslime.Account.load('seedsuserbbb', privateKey, 'active');

describe('Organization', function(eoslim){

	this.timeout(15000);

	let accountsContract;
	let configContract;
	let organizationContract;
	let tokenContract;
	let firstuserOrg;
	let seconduserOrg;

	before(async () => {

		accountsContract = await eoslim.Contract.at('accts.seeds', accounts);
		configContract = await eoslim.Contract.at('settgs.seeds', config);
		organizationContract = await eoslim.Contract.at('orgtns.seeds', organization);
		tokenContract = await eoslim.Contract.at('token.seeds', token);
		firstuserOrg = await eoslim.Contract.at('orgtns.seeds', firstuser);
		seconduserOrg = await eoslim.Contract.at('orgtns.seeds', seconduser);


		console.log('reset organization');
		await organizationContract.reset();

		console.log('reset token stats');
		await tokenContract.resetweekly();

		console.log('reset accounts');
		await accountsContract.reset();
		
		console.log('configure');
		await configContract.configure('fee', 500000);

		console.log('join users');
		await accountsContract.adduser('seedsuseraaa', 'a user');
		await accountsContract.adduser('seedsuserbbb', 'another user');

		console.log('add rep');
		await accountsContract.addrep(firstuser.name, 10000);
		await accountsContract.addrep(seconduser.name, 13000);

	});

	it('Should open the balance accounts when some user transfers tokens', async () => {

		firstuserToken = await eoslim.Contract.at('token.seeds', firstuser);
		await firstuserToken.transfer(firstuser.name, organization.name, '150.0000 SEEDS', 'Initial supply');

		seconduserToken = await eoslim.Contract.at('token.seeds', seconduser);
		await seconduserToken.transfer(seconduser.name, organization.name, '50.0000 SEEDS', 'Initial supply');

		const initialBalance = await organizationContract.balances.limit(10).find();

		const expected = [ 
			{ account: 'orgtns.seeds', balance: '200.0000 SEEDS' },
			{ account: 'seedsuseraaa', balance: '150.0000 SEEDS' },
			{ account: 'seedsuserbbb', balance: '50.0000 SEEDS' } 
		]

		assert.deepEqual(expected, initialBalance, 'The initial balance is not right.');

	});

	it('Should create an organization', async () => {

		await firstuserOrg.create('testorg1', firstuser.name);
		await firstuserOrg.create('testorg2', firstuser.name);
		await seconduserOrg.create('testorg3', seconduser.name);

		const initialOrgs = await organizationContract.organization.limit(10).find();	

		const expected = [ 
			{ 
				org_name: 'testorg1',
		    	owner: 'seedsuseraaa',
			    status: 0,
			    regen: 0,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			},
		  	{ 
		  		org_name: 'testorg2',
			    owner: 'seedsuseraaa',
			    status: 0,
			    regen: 0,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			},
		  	{ 
		  		org_name: 'testorg3',
			    owner: 'seedsuserbbb',
			    status: 0,
			    regen: 0,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			}
		];

		assert.deepEqual(expected, initialOrgs, 'The initial organizations are not right.');

	});

	it('Should add members', async () => {

		await firstuserOrg.addmember('testorg1', firstuser.name, seconduser.name, 'admin');
		await seconduserOrg.addmember('testorg3', seconduser.name, firstuser.name, 'admin');

		const localProvider = eoslim.Provider;
		const members1 = await localProvider.select('members').from('orgtns.seeds').scope('testorg1').limit(10).find();
		const members2 = await localProvider.select('members').from('orgtns.seeds').scope('testorg3').limit(10).find();
		
		const expected1 = [ 
			{ 
				account: 'seedsuseraaa', 
				role: '' 
			},
  			{ 
  				account: 'seedsuserbbb', 
  				role: 'admin' 
  			} 
  		];

  		const expected2 = [ 
  			{ 
  				account: 'seedsuseraaa', 
  				role: 'admin' 
  			},
  			{ 
  				account: 'seedsuserbbb', 
  				role: '' 
  			} 
  		]

  		assert.deepEqual(expected1, members1, 'Members table is not right.');
  		assert.deepEqual(expected2, members2, 'Members table is not right.');

	});

	it('Should destroy organization', async () => {

		await firstuserOrg.destroy('testorg2', firstuser.name);
		await firstuserOrg.refund(firstuser.name, '50.0000 SEEDS');

		const orgs = await organizationContract.organization.limit(10).find();

		const expected = [ 
			{ 
				org_name: 'testorg1',
			    owner: 'seedsuseraaa',
			    status: 0,
			    regen: 0,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' },
			{ 
				org_name: 'testorg3',
			    owner: 'seedsuserbbb',
			    status: 0,
			    regen: 0,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			} 
		];

		assert.deepEqual(expected, orgs, 'Organizations table is not right.');

	});

	it('Should change owner', async () => {

		try{
			console.log('Trying to remove the organization owner')
			await firstuserOrg.removemember('testorg1', firstuser.name, firstuser.name);
		}
		catch(err){
			console.log('You can not remove the owner')
		}

		seconduserOrg.changeowner('testorg3', seconduser.name, firstuser.name);

		console.log('Change roles');
		firstuserOrg.changerole('testorg3', firstuser.name, seconduser.name, 'testrole');

	});

	it('Should remove member', async () => {

		await Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 4000);

		await firstuserOrg.removemember('testorg3', firstuser.name, seconduser.name);

		const localProvider = eoslim.Provider;
		const members = await localProvider.select('members').from('orgtns.seeds').scope('testorg3').limit(10).find();
		const expected = [ { account: 'seedsuseraaa', role: 'admin' } ]

		assert.deepEqual(expected, members, 'The members table is not right.');

	});

	it('Should add regen', async () => {

		await firstuserOrg.addregen('testorg1', firstuser.name);
		await seconduserOrg.subregen('testorg3', seconduser.name);

		const orgs = await organizationContract.organization.limit(10).find();
		console.log(orgs);

		const expected = [ 
			{
			 	org_name: 'testorg1',
			    owner: 'seedsuseraaa',
			    status: 0,
			    regen: 10000,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			},
			{ 
				org_name: 'testorg3',
			    owner: 'seedsuseraaa',
			    status: 0,
			    regen: -13000,
			    reputation: 0,
			    voice: 0,
			    fee: '50.0000 SEEDS' 
			} 
		];

		assert.deepEqual(expected, orgs, 'The organization table is not right.');

	});

});















