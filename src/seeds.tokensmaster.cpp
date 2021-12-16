#include <seeds.tokensmaster.hpp>



void tokensmaster::reset() {
  require_auth(_self);

  utils::delete_table<token_tables>(contracts::tokensmaster, contracts::tokensmaster.value);
  utils::delete_table<usecase_table>(contracts::tokensmaster, contracts::tokensmaster.value);
}

void tokensmaster::submittoken(name submitter, name usecase, string chain, name contract, symbol_code symbolcode, string json)
{
  require_auth(submitter);
  check(CHAINS.find(chain)!=CHAINS.end(), "invalid chain");
  check(is_account(contract), "no contract account");
  check(symbolcode.is_valid(), "invalid symbol");
  check(json.size() <= MAXJSONLENGTH, "json string is > "+std::to_string(MAXJSONLENGTH));
  usecase_table usecasetable(get_self(), get_self().value);
  usecasetable.get(usecase.value, "invalid use case");  

  string signature = submitter.to_string()+usecase.to_string()+chain+contract.to_string()+symbolcode.to_string();
  token_tables tokentable(get_self(), usecase.value);
  auto token_signature_index = tokentable.get_index<"signature"_n>();
  check(token_signature_index.find(eosio::sha256( signature.c_str(), signature.length())) == token_signature_index.end(),
                                   "token already submitted");
  stats statstable( contract, symbolcode.raw() );
  const auto& st = statstable.get( symbolcode.raw(), ("no symbol "+symbolcode.to_string()+" in "+contract.to_string()).c_str());
  token_table tt;
  tt.submitter = submitter;
  tt.usecase = usecase;
  tt.contract = contract;
  tt.symbolcode = symbolcode;
  tt.approved = false;
  tt.chainName = chain;
  tt.name = symbolcode.to_string();
  tt.logo = DEFAULTLOGO;
  tt.backgroundImage = DEFAULTBACKGROUND;
  tt.balanceSubTitle = "Wallet Balance";
  tt.precision = 4;
  tt.extrajson = "{}";

  /* parse json, validate, insert into structure */

  if(usecase==LIGHTWALLET) {
    for( auto itr = tokentable.begin(); itr != tokentable.end(); itr++ ) {
      check(itr->symbolcode != symbolcode, "duplicate symbol code");
    }
  }

  tokentable.emplace(submitter, [&]( auto& s ) {
    s = tt;
    s.id = tokentable.available_primary_key();
  });
}

void tokensmaster::approvetoken(name submitter, name usecase, string chain, name contract, symbol_code symbolcode, bool approve)
{
  require_auth(get_self());

  string signature = submitter.to_string()+usecase.to_string()+usecase.to_string()+chain+contract.to_string()+symbolcode.to_string();
  token_tables tokentable(get_self(), usecase.value);
  auto token_signature_index = tokentable.get_index<"signature"_n>();
  const auto& tt = token_signature_index.get( eosio::sha256( signature.c_str(), signature.length()),
                                             "no matching row to approve" );
  if(approve) {
    tokentable.modify (tt, get_self(), [&]( auto& s ) {
      s.approved = true;
    });
  } else {
    tokentable.erase(tt);
  }
}

void tokensmaster::usecase(name usecase, bool add)
{
  require_auth(get_self());
  usecase_table usecasetable(get_self(), get_self().value);
  const auto& uc = usecasetable.find(usecase.value);
  if(add && uc == usecasetable.end()) {
    usecasetable.emplace(get_self(), [&]( auto& s ) {
      s.usecase = usecase;
    });
  } else {
    check(uc != usecasetable.end(), ("usecase "+usecase.to_string()+"does not exist").c_str());
    usecasetable.erase(uc);
  }
}
  
    
