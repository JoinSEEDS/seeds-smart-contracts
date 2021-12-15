#include <seeds.tokensmaster.hpp>



void tokensmaster::reset() {
  require_auth(_self);

  utils::delete_table<token_tables>(contracts::tokensmaster, contracts::tokensmaster.value);
  utils::delete_table<usecase_table>(contracts::tokensmaster, contracts::tokensmaster.value);
}

void tokensmaster::submittoken(name submitter, name scope, name contract, symbol_code symbolcode, string json)
{
  require_auth(submitter);
  //check(scope.is_valid(), "invalid scope name");
  check(is_account(contract), "no contract account");
  check(symbolcode.is_valid(), "invalid symbol");
  check(json.size() <= MAXJSONLENGTH, "json string is > "+std::to_string(MAXJSONLENGTH));

  string signature = submitter.to_string()+scope.to_string()+contract.to_string()+symbolcode.to_string();
  token_tables tokentable(get_self(), scope.value);
  auto token_signature_index = tokentable.get_index<"signature"_n>();
  check(token_signature_index.find(eosio::sha256( signature.c_str(), signature.length())) == token_signature_index.end(),
                                   "token already submitted");
  stats statstable( contract, symbolcode.raw() );
  const auto& st = statstable.get( symbolcode.raw(), ("no symbol "+symbolcode.to_string()+" in "+contract.to_string()).c_str());
  token_table tt;
  tt.submitter = submitter;
  tt.scope = scope;
  tt.contract = contract;
  tt.symbolcode = symbolcode;
  tt.approved = false;
  tt.chainName = "Telos";
  tt.name = symbolcode.to_string();
  tt.logo = DEFAULTLOGO;
  tt.backgroundImage = DEFAULTBACKGROUND;
  tt.balanceSubTitle = "Wallet Balance";
  tt.precision = 4;
  tt.extrajson = "{}";

  /* parse json, validate, insert into structure */

  if(scope==LIGHTWALLET) {
    for( auto itr = tokentable.begin(); itr != tokentable.end(); itr++ ) {
      check(itr->symbolcode != symbolcode, "duplicate symbol code");
    }
  }

  tokentable.emplace(submitter, [&]( auto& s ) {
    s = tt;
    s.id = tokentable.available_primary_key();
  });
}

void tokensmaster::approvetoken(name submitter, name scope, name contract, symbol_code symbolcode, bool approve)
{
  require_auth(get_self());
  check(is_account(submitter), "no submitter account");
  //check(scope.is_valid(), "invalid scope name");
  check(is_account(contract), "no contract account");
  check(symbolcode.is_valid(), "invalid symbol");

  string signature = submitter.to_string()+scope.to_string()+contract.to_string()+symbolcode.to_string();
  token_tables tokentable(get_self(), scope.value);
  auto token_signature_index = tokentable.get_index<"signature"_n>();
  const auto& tt = token_signature_index.get( eosio::sha256( signature.c_str(), signature.length()),
                                             "no matching row to approve" );
  if(approve) {
    tokentable.modify (tt, same_payer, [&]( auto& s ) {
      s.approved = true;
    });
  } else {
    tokentable.erase(tt);
  }
}

