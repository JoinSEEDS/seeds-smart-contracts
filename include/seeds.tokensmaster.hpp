#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp> 
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/singleton.hpp>
#include "contracts.hpp"
#include "tables.hpp"
#include "utils.hpp"

using namespace eosio;
using std::string;
   /**
    * The `tokensmaster' contract implements a master library of token metadata for tokens used in the Seeds ecosystems tools.
    *
    * This specifically addresses the use case of Light Wallet which requires a curated list of tokens which are visible
    *   to users in LW and which must have unique symbol codes. Metadata needed for the LW UI include user-friendly name,
    *   logo image, background, and balance title string.
    * Other wallets/applications (e.g. Peer Swaps) are potentially supported through additional "usecase" names.
    *
    * The `config` table is a singleton identifying the blockchain and the manager account
    *
    * The `tokens` table contains one row per submitted token with fields for token identity and for json metadata.
    *
    * The `usecases` table contains one row per usecase
    *
    * The `acceptances` table contains one row for each token acceptable for a usecase and is scoped to usecase name.
    *
    * The `blacklist` table contains symbol codes which are not allowed (unless specifically whitelisted)
    *
    * The `whitelist` table contains chain/token entries which are allowed (despite being duplicates or blacklisted).
    *
    * New tokens are submitted to the master list without a vetting process, but spam is discouraged due to a RAM
    *  requirement. An acceptance may be performed by the manager account or by a curator assigned to a specific
    *  use case. It is expected that an application (associated to a usecase) will only recognize "accepted" token
    *  entries.
    *
    * This contract does not accept submission of duplicate token entries. It is the manager's/curator's
    * responsibility not to accept erroneously or maliciously submitted token metadata.
    */

CONTRACT tokensmaster : public contract {
  public:
      using contract::contract;
      tokensmaster(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
          configs(receiver, receiver.value)
          {}

      /**
          * The `reset` action executed by the tokensmaster contract account deletes all table data
      */
      ACTION reset();

      /**
          * The one-time `init` action executed by the tokensmaster contract account records
          *  the chain name and manager account; it also specifies whether the contract
          *  should verify that a token contract exists when a token is submitted
          *
          * @param chain - the conventional string name for the chain (e.g. "Telos"),
          * @param manager - an account empowered to execute `accepttoken` and `deletetoken` actions
          * @param verify - if true, test that token contract & supply exist on `submittoken` action
      */
      ACTION init(string chain, name manager, bool verify);

      /**
          * The `submittoken` action executed by the `submitter` account places a new row into the
          * `tokens` table
          *
          * @param submitter - the account that submits the token,
          * @param chain - identifier of chain (e.g. "Telos")
          * @param contract - the account name of the token contract,
          * @param symbolcode - the symbol code for the token,
          * @param json - the metadata for the token
          *
          * @pre submitter must be a valid account with authorization for the transaction,
          * @pre submitter account must own sufficient RAM to support the transaction,
          * @pre chain must be <= 32 characters,
          * @pre if config 'verify' flag is true, and the token is not on the whitelist, a
          *        check is made that the contract is a valid account on this chain with a
          *        token contract matching symbolcode;
          *      if flag is false no contract check is made.
          * @pre json must be <= 2048 characters. Note that the contract does not validate the json.
          * @pre the token must satisfy either
          *       (a) the token is on the whitelist, or
          *       (b) its symbol neither (i) is on the blacklist, nor (ii) duplicates an existing entry
      */
      ACTION submittoken(name submitter, string chain, name contract, symbol_code symbolcode, string json);

      /**
          * The `accepttoken` action executed by the manager or contract account adds or removes a
          * `acceptances` table row indicating that a particular token is accepted for that usecase.
          * A new usecase is created if one does not exist; a usecase is deleted if its last token is removed.
          *
          * @param id - token identifier, from the row id of the tokens table,
          * @param symbolcode - the symbol code for the token,
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param accept - boolean: if true, add the new row;
          *                          if false, delete the row.
          *
          * @pre id must exist in the `tokens` table and match the symbolcode
          * @pre if 'accept' is false, the row must exist in the `acceptances` table;
          *       if true, the row must not exist.
          * @pre the tokensmaster contract account must own sufficient RAM to support the
          *   transaction
      */
      ACTION accepttoken(uint64_t id, symbol_code symbolcode, name usecase, bool accept);

      /**
          * The `deletetoken` action executed by the submitter or manager account deletes a
          * token from the token table
          *
          * @param id - token identifier, from the row id of the tokens table,
          * @param symbolcode - the symbol code for the token
          *
          * @pre id must exist in the `tokens` table and match the symbolcode
          * @pre the id must not be referenced in any acceptance table for any usecase
      */
      ACTION deletetoken(uint64_t id, symbol_code symbolcode);

      /**
          * The `setcurator` action, executed by the manager account or an
          * existing curator account, sets a new value for the curator account
          * associated with a specific usecase. (As a special case, this action
          * will update the manager account name if called with a null usecase.)
          *
          * @param usecase - an existing usecase, or null name 
          * @param curator - the account name 
          *
          * @pre usecase must be in the usecases table, or null
          * @pre curator account must exist on the Telos chain
      */
      ACTION setcurator(name usecase, name curator);

      /**
          * The `updblacklist` (update blacklist) action executed by manager account
          * (or by contract account prior to initialization with the `init` action)
          * adds or deletes a symbol from the blacklist table
          *
          * @param symbolcode - a symbol code for a token
          * @param add - if true, add the symbol code to the blacklist
          *              if false, delete the symbol code from the blacklist
          *
          * @pre if `add` is true, symbol code must not be on the blacklist
          * @pre if `add` is false, symbol code must be on the blacklist
      */
      ACTION updblacklist(symbol_code symbolcode, bool add);

      /**
          * The `updwhitelist` (update whitelist) action executed by manager account
          * (or by contract account prior to initialization with the `init` action)
          * adds or deletes a token from the whitelist table
          *
          * @param chain - the blockchain name (e.g Telos)
          * @param token - the token, specified by symbol and contract
          * @param add - if true, add the token to the whitelist
          *              if false, delete the token from the whitelist
          *
          * @pre if `add` is true, token must not be on the whitelist
          * @pre if `add` is false, token must be on the whitelist
      */
      ACTION updwhitelist(string chain, extended_symbol token, bool add);


  private:
      const uint16_t MAXJSONLENGTH = 2048;
      string json_schema();

      TABLE config { // single table, singleton, scoped by contract account name
        string             chain;
        name               manager;
        bool               verify;
        time_point         init_time;
        // binary_extension<> needed temporarily for migration
        binary_extension<time_point>
                           changed_time;
      } config_row;

      TABLE token_table { // single table, scoped by contract account name
        uint64_t id;
        name submitter;
        string chainName;
        name contract;
        symbol_code symbolcode;
        string json;

        uint64_t primary_key() const { return id; }
        uint64_t by_sym_code() const { return symbolcode.raw(); }
      };

      TABLE usecases { // single table, scoped by contract account name
        name   usecase;
        name   curator;
        uint64_t primary_key() const { return usecase.value; }

      };

      TABLE acceptances { // scoped by usecase name
        uint64_t   token_id;
 
        uint64_t primary_key() const { return token_id; }

      };

      TABLE blacklist { // single table, scoped by contract account name
        symbol_code        sym_code;

        uint64_t primary_key() const { return sym_code.raw(); }
      };

      TABLE whitelist { // single table, scoped by contract account name
        uint64_t           id;
        string             chainName;
        extended_symbol    token;

        uint64_t primary_key() const { return id; }
        uint64_t by_sym_code() const { return token.get_symbol().code().raw(); }

      };

      TABLE currency_stats {  // from standard token contract
         asset    supply;
         asset    max_supply;
         name     issuer;

         uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };

      TABLE schema { // single table, singleton, scoped by contract account name
        string             schema;
      } schema_row;

    typedef eosio::singleton< "config"_n, config > config_table;
    config_table configs;
    typedef eosio::multi_index< "config"_n, config >  dump_for_config;

    typedef eosio::multi_index<"tokens"_n, token_table, indexed_by
               < "symcode"_n,
                 const_mem_fun<token_table, uint64_t, &token_table::by_sym_code >
               >  > token_tables;
    
    typedef eosio::multi_index<"usecases"_n, usecases> usecase_table;

    typedef eosio::multi_index<"acceptances"_n, acceptances> acceptance_table;

    typedef eosio::multi_index< "blacklist"_n, blacklist > black_table;

    typedef eosio::multi_index< "whitelist"_n, whitelist, indexed_by
               < "symcode"_n,
                 const_mem_fun<whitelist, uint64_t, &whitelist::by_sym_code >
               > >  white_table;

    typedef eosio::multi_index< "stat"_n, currency_stats > stats;

    typedef eosio::singleton< "schema"_n, schema > schema_table;
    typedef eosio::multi_index< "schema"_n, schema >  dump_for_schema;

};


