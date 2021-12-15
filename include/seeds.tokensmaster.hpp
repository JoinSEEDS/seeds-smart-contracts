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
    * This addresses the use case of Light Wallet which requires a curated list of tokens which are visible to users in LW
    *   and which must have unique symbol codes. Metadata needed for the LW UI include user-friendly name, logo image, background,
    *   and balance title string.
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
          * @param scope - identifier of use case (e.g. `lightwallet`),
          * @param contract - the account name of the token contract,
          * @param symbolcode - the symbol code for the token,
          * @param json - the metadata for the token
          *
          * @pre submitter must be a valid account with authorization for the transaction,
          * @pre scope must be a valid eosio name string
          * @pre contract must be a valid account with a token contract matching symbolcode
          * @pre json must contain the required fields and be <= 2048 characters,
          * @pre there must not already be a row in the `tokens` table with matching parameters
      */
      ACTION submittoken(name submitter, name scope, name contract, symbol_code symbolcode, string json);


      /**
          * The `approvetoken` action executed by the tokensmaster contract account refers to an existing row
          * in the `tokens` table and either (1) sets the approved field to true, or (2) deletes the row
          * from the `tokens` table.
          *
          * @param submitter - the account that submitted the token,
          * @param scope - identifier of use case (e.g. `lightwallet`),
          * @param contract - the account name of the token contract,
          * @param symbolcode - the symbol code for the token,
          * @param approve - boolean: if true, accept the submitted token; if false, delete the token
          *
          * @pre submitter, scope, contract, and symbolcode must match an existing row in the `tokens` table
      */
      ACTION approvetoken(name submitter, name scope, name contract, symbol_code symbolcode, bool approve);


  private:
      const uint16_t MAXJSONLENGTH = 2048;
      const string DEFAULTLOGO = "logourl"; //TODO
      const string DEFAULTBACKGROUND = "backgroundurl"; //TODO
      const name LIGHTWALLET = "lightwallet"_n;
      TABLE token_table { // scoped by usecase (e.g. 'lightwallet'_n)
        uint64_t id;
        name submitter;
        name scope;
        name contract;
        symbol_code symbolcode;
        bool approved;
        string chainName;
        string name;
        string logo;
        string backgroundImage;
        string balanceSubTitle;
        uint8_t precision; // TODO: is this just UI preferred display precision or should it always match token asset?
        string extrajson;

        uint64_t primary_key() const { return id; }
        eosio::checksum256 by_secondary() const {
               string signature = submitter.to_string()+scope.to_string()+contract.to_string()+symbolcode.to_string();
               return eosio::sha256( signature.c_str(), signature.length());
            }
      };

      TABLE usecase { // singleton, scoped by contract account name
        name usecase;

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
       const_mem_fun<token_table, checksum256, &token_table::by_secondary>>
     > token_tables;
    
    typedef eosio::multi_index<"usecases"_n, usecase> usecase_table;

    typedef eosio::multi_index< "stat"_n, currency_stats > stats;
};

EOSIO_DISPATCH(tokensmaster, (reset)(submittoken)(approvetoken));

