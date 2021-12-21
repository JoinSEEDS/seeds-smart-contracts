#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
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
    * Other wallets/applications are potentially supported through additional "usecase" entries.
    *
    * The `tokens` table contains one row per token with fields for token identity and for each metadata item. It is scoped by use
    *   case (e.g. 'lightwallet').
    *
    * The `usecases` table contains one row for each use case and is scoped to the tokensmaster contract account name.
    *
    * Adding new tokens to the master list is done through a sumbit/approve sequence
    */

CONTRACT tokensmaster : public contract {
  public:
      using contract::contract;

      /**
          * The `reset` action executed by the tokensmaster contract account deletes all table data
      */
      ACTION reset();

      /**
          * The `submittoken` action executed by the `submitter` account places a new row into the
          * `tokens` table, with the approved field set to false
          *
          * @param submitter - the account that submits the token,
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param chain - identifier of chain (e.g. "Telos")
          * @param contract - the account name of the token contract,
          * @param symbolcode - the symbol code for the token,
          * @param json - the metadata for the token
          *
          * @pre submitter must be a valid account with authorization for the transaction,
          * @pre usecase must be a valid eosio name already existing in the `usecases` table
          * @pre chain must be a known chain id string ("Telos", "EOS", "WAX", etc.)
          * @pre contract must be a valid account with a token contract matching symbolcode
          * @pre json must contain the required fields and be <= 2048 characters; required fields are
          *       "name", "logo", "backgroundImage", "balanceSubTitle", "precision".
          * @pre there must not already be a row in the `tokens` table with matching parameters
      */
      ACTION submittoken(name submitter, name usecase, string chain, name contract, symbol_code symbolcode, string json);


      /**
          * The `approvetoken` action executed by the tokensmaster contract account refers to an existing row
          * in the `tokens` table and either (1) sets the approved field to true, or (2) deletes the row
          * from the `tokens` table.
          *
          * @param submitter - the account that submitted the token,
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param chain - identifier of chain (e.g. "Telos")
          * @param contract - the account name of the token contract,
          * @param symbolcode - the symbol code for the token,
          * @param approve - boolean: if true, accept the submitted token; if false, delete the token
          *
          * @pre submitter, usecase, chain, contract, and symbolcode must match an existing row in the `tokens` table
      */
      ACTION approvetoken(name submitter, name usecase, string chain, name contract, symbol_code symbolcode, bool approve);

      /**
          * The `usecase` action executed by the tokensmaster contract account adds or removes an entry in
          * the `usecases` table and assigns an account to manage it.
          *
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param manager - an account with management authority over the usecase
          * @param add - boolean: if true, add the new row; if false, delete the row
          *
          * @pre usecase must be a valid eosio name
          * @pre manager must be an existing account
          * @pre if `add` is true then the row must not exist in the table
          * @pre if `add` is false then the row must exist in the table
      */
      ACTION usecase(name usecase, name manager, bool add);

      /**
          * The `usecasecfg` action executed by the usecase manager configures the 
          * the required json fields for a row of the `usecases` table.
          *
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param unique_symbols - if true, identical symbols on distinct contracts are disallowed
          * @param allowed_chain - a chain name (e.g. "Telos")
          * @param required_fields - a space-delimited string of field names
          *
          * @pre usecase must exist in the `usecases` table
          * @pre the transaction must have the active authority of the usecase manager
          * @pre `allowed_chain` must be 12 or fewer characters
          * @pre each substring in `required_fields` must be a valid eosio name
      */
      ACTION usecasecfg(name usecase, bool unique_symbols, string allowed_chain, string required_fields);


  private:
      const uint16_t MAXJSONLENGTH = 2048;
      TABLE token_table { // scoped by usecase (e.g. 'lightwallet'_n)
        uint64_t id;
        name submitter;
        name usecase;
        string chainName;
        name contract;
        symbol_code symbolcode;
        bool approved;
        string json;

        uint64_t primary_key() const { return id; }
        eosio::checksum256 by_signature() const {
               string signature = submitter.to_string()+usecase.to_string()+chainName
                                  +contract.to_string()+symbolcode.to_string();
               return eosio::sha256( signature.c_str(), signature.length());
            }
        uint64_t by_symbolcode() const { return symbolcode.raw(); }
      };

      TABLE usecase_ { // single table, scoped by contract account name
        name usecase;
        name manager;
        bool unique_symbols;
        string allowed_chain;
        string required_fields;

        uint64_t primary_key() const { return usecase.value; }

      };

      TABLE currency_stats {  // from standard token contract
         asset    supply;
         asset    max_supply;
         name     issuer;

         uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };

    typedef eosio::multi_index<"tokens"_n, token_table,
     indexed_by<"signature"_n,
       const_mem_fun<token_table, checksum256, &token_table::by_signature>>,
     indexed_by<"symbolcode"_n,
       const_mem_fun<token_table, uint64_t, &token_table::by_symbolcode>>
     > token_tables;
    
    typedef eosio::multi_index<"usecases"_n, usecase_> usecase_table;

    typedef eosio::multi_index< "stat"_n, currency_stats > stats;

      /**
          * The `check_json_fields` function performs limited string processing to test that expected fields
          * exist in a json string. The field names are keys in a std::map passed to the function.
          * The input string is parsed in sequence and the function returns when all fields have
          * been found, the input has been exhausted, or the function is unable to process further.
          * This function is not a general json processor and does not recognize objects {...} or non-
          * string json elements. Therefore the expected fields should occur before any non-string
          * element.
          *
          * @param result - a map<string, bool> with the bool values initialized to false
          * @param input - the json string to be scanned
          *
          * @return - a status string which is zero-length on success
          *
          * @pre the input string should not contain any escaped backslash \\ before a double quote "
      */
    string check_json_fields(std::map<string, bool>& field_list, const string& input);

};

EOSIO_DISPATCH(tokensmaster, (reset)(submittoken)(approvetoken)(usecase)(usecasecfg));

