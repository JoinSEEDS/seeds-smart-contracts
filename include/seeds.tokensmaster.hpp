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
          * @pre json must contain the required fields and be <= 2048 characters,
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
          * the `usecases` table.
          * Adding an existing usecase is allowed but has no effect.
          *
          * @param usecase - identifier of use case (e.g. `lightwallet`),
          * @param add - boolean: if true, add the new row; if false, delete the row
          *
          * @pre usecase must be a valid eosio name
          * @pre if `add` is false then the row must exist in the table
      */
      ACTION usecase(name usecase, bool add);


  private:
      const uint16_t MAXJSONLENGTH = 2048;
      const string DEFAULTLOGO = "logourl"; //TODO
      const string DEFAULTBACKGROUND = "backgroundurl"; //TODO
      const name LIGHTWALLET = "lightwallet"_n;
      const std::set<string> CHAINS = {"Telos", "EOS"};
      TABLE token_table { // scoped by usecase (e.g. 'lightwallet'_n)
        uint64_t id;
        name submitter;
        name usecase;
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
        eosio::checksum256 by_signature() const {
               string signature = submitter.to_string()+usecase.to_string()+chainName
                                  +contract.to_string()+symbolcode.to_string();
               return eosio::sha256( signature.c_str(), signature.length());
            }
        uint64_t by_symbolcode() const { return symbolcode.raw(); }
      };

      TABLE usecase_ { // singleton, scoped by contract account name
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
       const_mem_fun<token_table, checksum256, &token_table::by_signature>>,
     indexed_by<"symbolcode"_n,
       const_mem_fun<token_table, uint64_t, &token_table::by_symbolcode>>
     > token_tables;
    
    typedef eosio::multi_index<"usecases"_n, usecase_> usecase_table;

    typedef eosio::multi_index< "stat"_n, currency_stats > stats;
};

EOSIO_DISPATCH(tokensmaster, (reset)(submittoken)(approvetoken));

