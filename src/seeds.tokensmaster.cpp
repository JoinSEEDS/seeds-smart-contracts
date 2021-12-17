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
  tt.json = json;

  std::map<string, string> mapoffields = {
    {"name", ""},
    {"logo", ""},
    {"backgroundImage", ""},
    {"balanceSubTitle", ""},
    {"precision", ""}
  };

  string rv = skim_json(mapoffields, json);
  check(rv.empty(), ("couldn't parse json: "+rv).c_str());
  tt.name = mapoffields["name"];
  tt.logo = mapoffields["logo"];
  tt.backgroundImage = mapoffields["backgroundImage"];
  tt.balanceSubTitle = mapoffields["balanceSubTitle"];
  tt.precision = stoi(mapoffields["precision"]);

  if(usecase==LIGHTWALLET) {
    auto token_symbolcode_index = tokentable.get_index<"symbolcode"_n>();
    check(token_symbolcode_index.find(symbolcode.raw()) == token_symbolcode_index.end(),
          "duplicate symbol code");
    check(chain=="Telos", "chain must be Telos");
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
  
string tokensmaster::skim_json(std::map<string, string>& result, const string& input)
{
  std::string key, value;
  size_t cp, s;
  int keysremain = result.size();
  cp = input.find_first_of("{");
  if (cp==string::npos) { 
    return "no open brace";
  }
  while(keysremain) {
    cp = input.find_first_of("\"", cp+1);
    if (cp==string::npos) { return "expected \""; }
    s = cp+1;
    cp = input.find_first_of("\"", cp+1);
    key = input.substr(s, cp-s);
    cp = input.find_first_of(":", cp+1);
    cp = input.find_first_of("{,\"", cp+1);
    s = cp+1;
    if(input[cp]==',') { continue; }
    if(input[cp]=='{') {
      return "reached {";
    }
    do {
      cp = input.find_first_of("\"", cp+1);
    } while (input[cp-1]=='\\');
    value = input.substr(s, cp-s);
    if ( result.find(key) != result.end() ) {
      result[key] = value;
      --keysremain;
    }
    cp = input.find_first_of(",}", cp+1);
    if(input[cp]=='}') { return "incomplete by "+std::to_string(keysremain); }
  }
  return "";
}
   
