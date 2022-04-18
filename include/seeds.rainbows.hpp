#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>

#include <string>

using namespace eosio;

   using std::string;

   /**
    * The `rainbows` experimental contract implements the functionality described in the design document
    * https://rieki-cordon.medium.com/1fb713efd9b1 .
    * In the development process we are building on the eosio.token code. 
    *
    * The token contract defines the structures and actions that allow users to create, issue, and manage
    * tokens for EOSIO based blockchains. It exemplifies one way to implement a smart contract which
    * allows for creation and management of tokens.
    * 
    * The `rainbows` contract class also implements a public static method: `get_balance`. This allows
    * one to check the balance of a token for a specified account.
    * 
    * The `rainbows` contract manages the set of tokens, stakes, accounts and their corresponding balances,
    * by using four internal tables: the `accounts`, `stats`, `configs`, and `stakes`. The `accounts`
    * multi-index table holds, for each row, instances of `account` object and the `account` object
    * holds information about the balance of one token. The `accounts` table is scoped to an eosio
    * account, and it keeps the rows indexed based on the token's symbol.  This means that when one
    * queries the `accounts` multi-index table for an account name the result is all the tokens under
    * this contract that account holds at the moment.
    * 
    * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each
    * row, which contains information about current supply, maximum supply, and the creator account.
    * The `stats` table is scoped to the token symbol_code. Therefore, when one queries the `stats`
    * table for a token symbol the result is one single entry/row corresponding to the queried symbol
    * token if it was previously created, or nothing, otherwise.
    *
    * The first two tables (`accounts` and `stats`) are structured identically to the `eosio.token`
    * tables, making "rainbow tokens" compatible with most EOSIO wallet and block explorer applications.
    * The two remaining tables (`configs` and `stakes`) provide additional data specific to the rainbow
    * token.
    *
    * The `configs` singleton table contains names of administration accounts (e.g. membership_mgr,
    * freeze_mgr) and some configuration flags. The `configs` table is scoped to the token symbol_code
    * and has a single row per scope.
    *
    * The `stakes` table contains staking relationships (staked currency, staking ratio, escrow account).
    * It is scoped by the token symbol_code and may contain 1 or more rows. It has a secondary index
    * based on the staked currency type.
    *
    * In addition, the `displays` singleton table contains json metadata intended for applications
    * (e.g. wallets) to use in UI display, such as a logo symbol url. It is scoped by token symbol_code.
    *
    * The `symbols` table is a housekeeping list of all the tokens managed by the contract. It is
    * scoped to the contract.
    */

   CONTRACT rainbows : public contract {
      public:
         using contract::contract;
         rainbows( name receiver, name code, datastream<const char*> ds )
         : contract( receiver, code, ds ),
         symboltable( receiver, receiver.value )
         {}

         /**
          * The ` create` action allows `issuer` account to create or reconfigure a token with the
          * specified characteristics. 
          * If the token does not exist, a new row in the stats table for token symbol scope is created
          * with the specified characteristics. At creation, its' approval flag is false, preventing
          * tokens from being issued.
          * If a token of this symbol does exist and update is permitted, the characteristics are updated.
          *
          * @param issuer - the account that creates the token,
          * @param maximum_supply - the maximum supply set for the token,
          * @param withdrawal_mgr - the account with authority to withdraw tokens from any account,
          * @param withdraw_to - the account to which withdrawn tokens are deposited,
          * @param freeze_mgr - the account with authority to freeze transfer actions,
          * @param redeem_locked_until - an ISO8601 date string; user redemption of stake is
          *   disallowed until this time; blank string is equivalent to "now" (i.e. unlocked).
          * @param config_locked_until - an ISO8601 date string; changes to token characteristics
          *   are disallowed until this time; blank string is equivalent to "now" (i.e. unlocked).
          * @param membership_symbol - a frozen "sister" token, also managed by this contract;
          *   a balance of 1 or 2 sister tokens classifies an account as "visitor" or "regular" member
          * @param broker_symbol - a frozen "sister" token, also managed by this contract;
          *   a balance of the sister token qualifies an account as holding the "broker" badge
          * @param cred_limit_symbol - a frozen "sister" token, also managed by this contract;
          *   a positive balance in the sister token will permit a user to overspend to that amount
          * @param pos_limit_symbol - a frozen "sister" token, also managed by this contract;
          *   no user transfer is allowed to increase the user balance over the sister token balance.
          *
          * @pre Token symbol has to be valid,
          * @pre Token symbol must not be already created, OR if it has been created,
          *   the config_locked field in the configtable row must be in the past,
          * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 2^62 - 1.
          * @pre Maximum supply must be positive,
          * @pre withdrawal manager must be an existing account,
          * @pre withdraw_to must be an existing account,
          * @pre freeze manager must be an existing account,
          * @pre redeem_locked_until must specify a time within +100/-10 yrs of now;
          * @pre config_locked_until must specify a time within +100/-10 yrs of now;
          * @pre membership_symbol must be an existing frozen token of zero precision on this contract, or empty
          * @pre broker_symbol must be an existing frozen token of zero precision on this contract, or empty
          * @pre cred_limit_symbol must be an existing frozen token of matching precision on this contract, or empty
          * @pre pos_limit_symbol must be an existing frozen token of matching precision on this contract, or empty
          
          */
         ACTION create( const name&   issuer,
                        const asset&  maximum_supply,
                        const name&   withdrawal_mgr,
                        const name&   withdraw_to,
                        const name&   freeze_mgr,
                        const string& redeem_locked_until,
                        const string& config_locked_until,
                        const string& membership_symbol,
                        const string& broker_symbol,
                        const string& cred_limit_symbol,
                        const string& pos_limit_symbol );


         /**
          * By this action the contract owner approves or rejects the creation of the token. Until
          * this approval, no tokens may be issued. If rejected, and no issued tokens are outstanding,
          * the table entries for this token are deleted.
          *
          * @param symbolcode - the symbol_code of the token to execute the close action for.
          * @param reject_and_clear - if this flag is true, delete token; if false, approve creation
          *
          * @pre The symbol must have been created.
          */
         ACTION approve( const symbol_code& symbolcode, const bool& reject_and_clear );


         /**
          * Allows `issuer` account to create a staking relationship for a token. A new row in the
          * stakes table for token symbol scope gets created with the specified characteristics.
          *
          * @param token_bucket - a reference quantity of the token,
          * @param stake_per_bucket - the number of stake tokens (e.g. Seeds) staked per "bucket" of tokens,
          * @param stake_token_contract - the staked token contract account (e.g. token.seeds),
          * @param stake_to - the escrow account where stake is held
          * @param proportional - redeem by proportion of escrow rather than by staking ratio.
          * @param reserve_fraction - minimum reserve ratio (as percent) of escrow balance to redemption liability.
          * @param memo - the memo string to accompany the transaction.
          *
          * @pre Token symbol must have already been created by this issuer
          * @pre The config_locked_until field in the configs table must be in the past,
          * @pre issuer must have a (possibly zero) balance of the stake token,
          * @pre stake_per_bucket must be non-negative
          * @param reserve_fraction must be non-negative
          * @pre issuer active permissions must include rainbowcontract@eosio.code
          * @pre stake_to active permissions must include rainbowcontract@eosio.code
          *
          * Note: the contract cannot internally check the required permissions status
          */
         ACTION setstake( const asset&      token_bucket,
                          const asset&      stake_per_bucket,
                          const name&       stake_token_contract,
                          const name&       stake_to,
                          const bool&       proportional,
                          const uint32_t&   reserve_fraction,
                          const string&     memo);

         /**
          * Allows `issuer` account to delete a staking relationship. Staked tokens are returned
          * to the issuer account. Deferred stake relationships are reverted. The row is removed
          * from the stakes table.
          *
          * @param stake_index - the index field in the `stakes` table
          * @param symbolcode - the staked token
          * @param memo - memo string
          *
          * @pre the config_locked_until field in the configs table must be in the past
          */
         ACTION deletestake( const uint64_t& stake_index,
                             const symbol_code& symbolcode,
                             const string& memo );


         /**
          * Allows `issuer` account to create or update display metadata for a token.
          * Issuer pays for RAM.
          * The currency_display table is intended for apps to access (e.g. via nodeos chain API).
          *
          * @param symbolcode - the token,
          * @param json_meta - json string of metadata. Minimum expected fields are
          *      name - human friendly name of token, max 32 char
          *      logo - url pointing to a small png or gif image (typ. 128x128 with transparency)
          *   Recommended fields are
          *      logo_lg - url pointing to a larger png or gif image (typ. 1024 x 1024)
          *      web_link - url pointing to a web page describing the token & application
          *      background - url pointing to a png or gif image intended as a UI background
          *                     (e.g. as used in Seeds Light Wallet)
          *   Additional fields are permitted within the overal length limit: max 2048 chars.
          *
          * @pre Token symbol must have already been created by this issuer
          * @pre String parameters must be within length limits
          */
         ACTION setdisplay( const symbol_code&  symbolcode,
                            const string&       json_meta
         );

         /**
          *  This action issues a `quantity` of tokens to the issuer account, and transfers
          *  a proportional amount of stake to escrow if staking is configured.
          *
          * @param quantity - the amount of tokens to be issued,
          * @memo - the memo string that accompanies the token issue transaction.
          * 
          * @pre The `approve` action must have been executed for this token symbol
          */
         ACTION issue( const asset& quantity, const string& memo );

         /**
          * The opposite for issue action, if all validations succeed,
          * it debits the statstable.supply amount. Any staked tokens are released from escrow in
          * proportion to the quantity of tokens retired.
          *
          * @param owner - the account containing tokens to retire,
          * @param quantity - the quantity of tokens to retire,
          * @param memo - the memo string to accompany the transaction.
          *
          * @pre the redeem_locked_until configuration must be in the past (except that
          *   this action is always permitted to the issuer.)
          * @pre If any staking relationships exist, for each relationship :
          *   1. the proportional unstaking flag must be configured true, OR
          *   2. the balance in the escrow account must meet the reserve_fraction criterion
          */
         ACTION retire( const name& owner, const asset& quantity, const string& memo );

         /**
          * Allows `from` account to transfer to `to` account the `quantity` tokens.
          * One account is debited and the other is credited with quantity tokens.
          *
          * @param from - the account to transfer from,
          * @param to - the account to be transferred to,
          * @param quantity - the quantity of tokens to be transferred,
          * @param memo - the memo string to accompany the transaction.
          * 
          * @pre The transfers_frozen flag in the configs table must be false, except for
          *   administrative-account transfers
          * @pre If configured with a membership_symbol in `create` operation, the sender and
          *   receiver must both be members, and at least one of them must be a regular member
          * @pre The `from` account balance must be sufficient for the transfer (allowing for
          *   credit if configured with credit_limit_symbol in `create` operation)
          * @pre If configured with positive_limit_symbol in `create` operation, the transfer
          *   must not put the `to` account over its maximum limit
          */
         ACTION transfer( const name&    from,
                          const name&    to,
                          const asset&   quantity,
                          const string&  memo );
         /**
          * Allows `ram_payer` to create an account `owner` with zero balance for
          * token `symbolcode` at the expense of `ram_payer`.
          *
          * @param owner - the account to be created,
          * @param symbolcode - the token symbol,
          * @param ram_payer - the account that supports the cost of this action.
          *
          * More information can be read [here](https://github.com/EOSIO/eosio.contracts/issues/62)
          * and [here](https://github.com/EOSIO/eosio.contracts/issues/61).
          */
         ACTION open( const name& owner, const symbol_code& symbolcode, const name& ram_payer );

         /**
          * This action is the opposite for open, it closes the account `owner`
          * for token `symbol`.
          *
          * @param owner - the owner account to execute the close action for,
          * @param symbolcode - the symbol of the token to execute the close action for.
          *
          * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
          * @pre If the pair of owner plus symbol exists, the balance has to be zero.
          */
         ACTION close( const name& owner, const symbol_code& symbolcode );

         /**
          * This action freezes or unfreezes transaction processing
          * for token `symbol`.
          *
          * @param symbol - the symbol of the token to execute the freeze action for.
          * @param freeze - boolean, true = freeze, false = enable transfers.
          * @param memo - the memo string to accompany the transaction.
          *
          * @pre The symbol has to exist otherwise no action is executed,
          * @pre Transaction must have the freeze_mgr authority 
          */
         ACTION freeze( const symbol_code& symbolcode, const bool& freeze, const string& memo );

         /**
          * This action clears RAM tables for all tokens. For a large deployment,
          * attempting to erase all table entries in one action might fail by exceeding the
          * chain execution time limit. The `limit` parameter protects against this. It is
          * advisable for the application to check the contract status (get_scope) to
          * discover whether a follow-up `reset` action is required.
          *
          * @param all - if true, clear all tables within the token scope;
          *              if false, keep accounts, stats, and symbols
          * @param limit - max number of erasures (for time control)
          *
          * @pre Transaction must have the contract account authority 
          */
         ACTION reset( const bool all, const uint32_t limit );

         /**
          * This action clears the `accounts` table for a particular account. All
          * token balances in the account are erased.
          *
          * @param account - account
          *
          * @pre Transaction must have the contract account authority 
          */
         ACTION resetacct( const name& account );

         static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto ac = accountstable.find( sym_code.raw() );
            if( ac == accountstable.end() ) {
               return null_asset;
            }
            return ac->balance;
         }

      private:
         const int max_stake_count = 8; // don't use too much cpu time to complete transaction
         const uint64_t no_index = static_cast<uint64_t>(-1); // flag for nonexistent defer_table link
         static const asset null_asset;
         const uint32_t VISITOR = 1;
         const uint32_t REGULAR = 2;

         TABLE account { // scoped on account name
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         TABLE currency_stats {  // scoped on token symbol code
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         TABLE currency_config {  // singleton, scoped on token symbol code
            name        withdrawal_mgr;
            name        withdraw_to;
            name        freeze_mgr;
            time_point  redeem_locked_until;
            time_point  config_locked_until;
            bool        transfers_frozen;
            bool        approved;
            symbol_code membership;
            symbol_code broker;
            symbol_code cred_limit;
            symbol_code positive_limit;
         };

         TABLE currency_display {  // singleton, scoped on token symbol code
            string     json_meta;
         };


         TABLE stake_stats {  // scoped on token symbol code
            uint64_t index;
            asset    token_bucket;
            asset    stake_per_bucket;
            name     stake_token_contract;
            name     stake_to;
            uint32_t reserve_fraction;
            bool     proportional;

            uint64_t primary_key()const { return index; };
            uint128_t by_secondary() const {
               return (uint128_t)stake_per_bucket.symbol.raw()<<64 | stake_token_contract.value;
            }
         };

         TABLE symbolt { // scoped on get_self()
            symbol_code  symbolcode;

            uint64_t primary_key()const { return symbolcode.raw(); };
         };

         typedef eosio::multi_index< "accounts"_n, account > accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;
         typedef eosio::singleton< "configs"_n, currency_config > configs;
         typedef eosio::multi_index< "configs"_n, currency_config >  dump_for_config;
         typedef eosio::singleton< "displays"_n, currency_display > displays;
         typedef eosio::multi_index< "displays"_n, currency_display >  dump_for_display;
         typedef eosio::multi_index< "stakes"_n, stake_stats, indexed_by
               < "staketoken"_n,
                 const_mem_fun<stake_stats, uint128_t, &stake_stats::by_secondary >
               >
            > stakes;
         typedef eosio::multi_index< "symbols"_n, symbolt > symbols;

         symbols symboltable;

         void sub_balance( const name& owner, const asset& value, const symbol_code& limit_symbol );
         void add_balance( const name& owner, const asset& value, const name& ram_payer,
                           const symbol_code& limit_symbol );
         void sister_check(const string& sym_name, uint32_t precision);
         void stake_all( const name& owner, const asset& quantity );
         void unstake_all( const name& owner, const asset& quantity );
         void stake_one( const stake_stats& sk, const name& owner, const asset& quantity );
         void unstake_one( const stake_stats& sk, const name& owner, const asset& quantity );
         void reset_one( const symbol_code symbolcode, const bool all, const uint32_t limit, uint32_t& counter );
 
   };

EOSIO_DISPATCH(rainbows,
   (create)(approve)(setstake)(deletestake)(setdisplay)(issue)(retire)(transfer)
   (open)(close)(freeze)(reset)(resetacct)
);

