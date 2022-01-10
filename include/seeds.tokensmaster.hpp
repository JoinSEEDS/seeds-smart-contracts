#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/singleton.hpp>
#include <contracts.hpp>
#include <tables.hpp>
#include <utils.hpp>

using namespace eosio;
using std::string;
   /**
    * The `tokensmaster' contract implements a master library of token metadata for tokens used in the Seeds ecosystems tools.
    *
    * This specifically addresses the use case of Light Wallet which requires a curated list of tokens which are visible
    *   to users in LW and which must have unique symbol codes. Metadata needed for the LW UI include user-friendly name,
    *   logo image, background, and balance title string.
    * Other wallets/applications (e.g. PTM) are potentially supported through additional "usecase" names.
    *
    * The `tokens` table contains one row per submitted token with fields for token identity and for each metadata item.
    *
    * The `acceptances` table contains one row for each token acceptable for a usecase and is scoped to usecase name.
    *
    * New tokens are submitted to the master list without a vetting process, but spam is discouraged due to a RAM
    *  requirement. An acceptance may be performed by the manager account. It is expected that an application
    *  (associated to a usecase) will only recognize "accepted" token entries.
    *
    * A submitter cannot update a token entry he/she previously submitted, but if the manager deletes
    * the token, a replacement entry may be submitted which will have a different numerical token_id.
    */

CONTRACT tokensmaster : public contract {
  public:
      using contract::contract;

      /**
          * The `reset` action executed by the tokensmaster contract account deletes all table data
      */
      ACTION reset();

      /**
          * The one-time `init` action executed by the tokensmaster contract account records
          *  the chain name and code hash in the config table; it also specifies whether the contract
          *  should verify that a token contract exists when a token is submitted
          *
          * @param chain - the conventional string name for the chain (e.g. "Telos"),
          * @param code_hash - the code hash registered on-chain with the "set code" transaction,
          * @param verify - if true, test that token contract & supply exist on `submittoken` action
          * @param manager - an account empowered to execute `accepttoken` and `deletetoken` actions
      */
      ACTION init(string chain, eosio::checksum256 code_hash, bool verify, name manager);

      /**
          * The `submittoken` action executed by the `submitter` account places a new row into the
          * `tokens` table, with the approved field set to false
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
          * @pre contract must be a valid account with a token contract matching symbolcode
          * @pre json must be <= 2048 characters. Note that the contract does not validate the json.
          * @pre there must not already be a row in the `tokens` table with matching parameters
      */
      ACTION submittoken(name submitter, string chain, name contract, symbol_code symbolcode, string json);

      /**
          * The `accepttoken` action executed by the manager account adds or removes a
          * `usecases` table row indicating that a particular token is accepted for that usecase.
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
          * The `deletetoken` action executed by the manager account deletes a token
          * and any associates usecase acceptance.
          *
          * @param id - token identifier, from the row id of the tokens table,
          * @param symbolcode - the symbol code for the token
          *
          * @pre id must exist in the `tokens` table and match the symbolcode
      */
      ACTION deletetoken(uint64_t id, symbol_code symbolcode);


  private:
      const uint16_t MAXJSONLENGTH = 2048;

      TABLE config { // single table, singleton, scoped by contract account name
        string             chain;
        eosio::checksum256 code_hash;
        bool               verify;
        time_point         init_time;
      } config_row;

      TABLE token_table { // single table, scoped by contract account name
        uint64_t id;
        name submitter;
        string chainName;
        name contract;
        symbol_code symbolcode;
        string json;

        uint64_t primary_key() const { return id; }
        eosio::checksum256 by_signature() const {
               string signature = submitter.to_string()+chainName
                                  +contract.to_string()+symbolcode.to_string();
               return eosio::sha256( signature.c_str(), signature.length());
            }
        uint64_t by_symbolcode() const { return symbolcode.raw(); }
      };

      TABLE usecases { // single table, scoped by contract account name
        name   usecase;

        uint64_t primary_key() const { return usecase.value; }

      };

      TABLE acceptances { // scoped by usecase name
        uint64_t   token_id;
 
        uint64_t primary_key() const { return token_id; }

      };

      TABLE currency_stats {  // from standard token contract
         asset    supply;
         asset    max_supply;
         name     issuer;

         uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };

    typedef eosio::singleton< "config"_n, config > config_table;
    typedef eosio::multi_index< "config"_n, config >  dump_for_config;

    typedef eosio::multi_index<"tokens"_n, token_table,
     indexed_by<"signature"_n,
       const_mem_fun<token_table, checksum256, &token_table::by_signature>>,
     indexed_by<"symbolcode"_n,
       const_mem_fun<token_table, uint64_t, &token_table::by_symbolcode>>
     > token_tables;
    
    typedef eosio::multi_index<"usecases"_n, usecases> usecase_table;

    typedef eosio::multi_index<"acceptances"_n, acceptances> acceptance_table;

    typedef eosio::multi_index< "stat"_n, currency_stats > stats;

};

EOSIO_DISPATCH(tokensmaster, (reset)(init)(submittoken)(accepttoken));

