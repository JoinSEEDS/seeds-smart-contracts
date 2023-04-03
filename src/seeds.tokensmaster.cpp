#include <seeds.tokensmaster.hpp>
#include <cstring>
#include <algorithm>

void tokensmaster::reset() {
  require_auth(get_self());
  usecase_table usecasetable(get_self(), get_self().value);
  for(auto itr = usecasetable.begin(); itr!=usecasetable.end(); ++itr) {  
    utils::delete_table<acceptance_table>(get_self(), itr->usecase.value);
  }
  utils::delete_table<usecase_table>(get_self(), get_self().value);
  utils::delete_table<token_tables>(get_self(), get_self().value);
  utils::delete_table<black_table>(get_self(), get_self().value);
  utils::delete_table<white_table>(get_self(), get_self().value);
  config_table configs(get_self(), get_self().value);
  configs.remove();
}

void tokensmaster::init(string chain, name manager, bool verify) {
  require_auth(get_self());
  schema_table schemas(get_self(), get_self().value);
  if( !schemas.exists() ) {
    auto schema_entry = schemas.get_or_create(get_self(), schema_row);
    schema_entry.schema = json_schema();
    schemas.set(schema_entry, get_self());
  }
  config_table configs(get_self(), get_self().value);
  check(!configs.exists(), "cannot re-initialize configuration");

  updblacklist(symbol_code("BTC"), true);
  updblacklist(symbol_code("ETH"), true);
  updblacklist(symbol_code("TLOS"), true);
  updblacklist(symbol_code("SEEDS"), true);
  updwhitelist("Telos", extended_symbol( symbol("SEEDS",4 ), "token.seeds"_n ), true);
  updwhitelist("Telos", extended_symbol( symbol("TLOS",4 ), "eosio.token"_n ), true);

  auto config_entry = configs.get_or_create(get_self(), config_row);
  check(chain.size() <= 32, "chain name too long");
  config_entry.chain = chain;
  config_entry.manager = manager;
  config_entry.verify = verify;
  config_entry.init_time = current_time_point();
  configs.set(config_entry, get_self());
}

void tokensmaster::submittoken(name submitter, string chain, name contract, symbol_code symbolcode, string json)
{
  config_table configs(get_self(), get_self().value);
  check(configs.exists(), "contract not initialized yet");
  require_auth(submitter);
  check(symbolcode.is_valid(), "invalid symbol");
  check(json.size() <= MAXJSONLENGTH, "json string too long, > "+std::to_string(MAXJSONLENGTH));
  token_tables tokentable(get_self(), get_self().value);
  white_table wtable(get_self(), get_self().value);
  auto widx = wtable.get_index<"symcode"_n>();
  auto itr = widx.find( symbolcode.raw() );
  bool white_match = false;
  for( ; itr!=widx.end(); ++itr ) {
    if( itr->token.get_contract() == contract && itr->chainName == chain) {
      white_match = true;
      break;
    }
  }
  if( !white_match ) {
    black_table btable(get_self(), get_self().value);
    const auto& bt = btable.find( symbolcode.raw() );
    check( bt == btable.end(), "symbol "+symbolcode.to_string()+" is blacklisted" );
    auto tidx = tokentable.get_index<"symcode"_n>();
    const auto& tt = tidx.find( symbolcode.raw() );
    check( tt == tidx.end(), "duplicate symbol: "+symbolcode.to_string() );
    if(configs.get().verify) {
      check(chain == configs.get().chain, "mismatched chain name");
      check(is_account(contract), "no contract account");
      stats statstable( contract, symbolcode.raw() );
      const auto& st = statstable.get( symbolcode.raw(), ("no symbol "+symbolcode.to_string()+" in "+contract.to_string()).c_str());
    }
  }
  tokentable.emplace(submitter, [&]( auto& s ) {
    s.id = tokentable.available_primary_key();
    s.submitter = submitter;
    s.chainName = chain;
    s.contract = contract;
    s.symbolcode = symbolcode;
    s.json = json;
  });
}

void tokensmaster::accepttoken(uint64_t id, symbol_code symbolcode, name usecase, bool accept)
{
  config_table configs(get_self(), get_self().value);
  check(configs.exists(), "contract not initialized yet");
  name manager = configs.get().manager;
  check(symbolcode.is_valid(), "invalid symbol");
  token_tables tokentable(get_self(), get_self().value);
  const auto& tt = tokentable.get(id, ("no match for token id "+std::to_string(id)).c_str());
  check(tt.symbolcode == symbolcode, "symbol doesn't match id");
  usecase_table usecasetable(get_self(), get_self().value);
  const auto& uc = usecasetable.find(usecase.value);
  if(uc == usecasetable.end()) {
    check(has_auth(manager), "not authorized");
  } else {
    check(has_auth(manager) || has_auth(uc->curator), "not authorized");
  }
  acceptance_table acceptancetable(get_self(), usecase.value);
  const auto& at = acceptancetable.find(id);
  if(accept) {
    check(at == acceptancetable.end(), "token already accepted");
    if(uc == usecasetable.end()) {
      usecasetable.emplace(get_self(), [&]( auto& s ) {
        s.usecase = usecase;
        s.curator = manager;
      });
    }
    acceptancetable.emplace(get_self(), [&]( auto& s ) {
      s.token_id = id;
    });
  } else {
    check(at != acceptancetable.end(), "token not found");
    acceptancetable.erase(at);
    if(acceptancetable.begin() == acceptancetable.end()) {
      usecasetable.erase(uc);
    }      
  }
}

void tokensmaster::deletetoken(uint64_t id, symbol_code symbolcode)
{
  config_table configs(get_self(), get_self().value);
  check(configs.exists(), "contract not initialized yet");
  name manager = configs.get().manager;
  check(symbolcode.is_valid(), "invalid symbol");
  token_tables tokentable(get_self(), get_self().value);
  const auto& tt = tokentable.get(id, ("no match for token id "+std::to_string(id)).c_str());
  check(tt.symbolcode == symbolcode, "symbol doesn't match id");
  check(has_auth(manager) || has_auth(tt.submitter), "not authorized");
  usecase_table usecasetable(get_self(), get_self().value);
  for(auto itr = usecasetable.begin(); itr != usecasetable.end(); itr++) {
    name usecase = itr->usecase;
    acceptance_table acceptancetable(get_self(), usecase.value);
    const auto& at = acceptancetable.find(id);
    check(at == acceptancetable.end(),
          ("cannot delete token, accepted by "+usecase.to_string()).c_str());

  }
  tokentable.erase(tt);
}

void tokensmaster::setcurator(name usecase, name curator)
{
  config_table configs(get_self(), get_self().value);
  check(configs.exists(), "contract not initialized yet");
  name manager = configs.get().manager;
  if( usecase == name() ) {
    require_auth(manager);
    check(is_account(curator), "account "+curator.to_string()+" does not exist");
    auto cf = configs.get();
    cf.manager = curator;
    configs.set( cf, manager );
    return;
  }
  usecase_table usecasetable(get_self(), get_self().value);
  const auto& uc = usecasetable.find(usecase.value);
  check( uc != usecasetable.end(), "usecase "+usecase.to_string()+" does not exist" );
  check(is_account(curator), "account "+curator.to_string()+" does not exist");
  check( has_auth(manager) || has_auth(uc->curator), "not authorized");
  usecasetable.modify( uc, same_payer, [&]( auto& s ) {
    s.curator = curator;
  });
}

void tokensmaster::updblacklist(symbol_code symbolcode, bool add)
{
  config_table configs(get_self(), get_self().value);
  if( configs.exists() ) {
    require_auth( configs.get().manager );
  } else {
    require_auth( get_self() );
  }
  check(symbolcode.is_valid(), "invalid symbol");
  black_table btable(get_self(), get_self().value);
  const auto& bt = btable.find( symbolcode.raw() );
  if( add ) {
    check( bt == btable.end(), "can't add "+symbolcode.to_string()+", already on blacklist." );
    btable.emplace(get_self(), [&]( auto& s ) {
      s.sym_code = symbolcode;
    });
  } else {
    check( bt != btable.end(), "can't delete "+symbolcode.to_string()+", not on blacklist." );
    btable.erase( bt );
  }
}

void tokensmaster::updwhitelist(string chain, extended_symbol token, bool add)
{
  config_table configs(get_self(), get_self().value);
  if( configs.exists() ) {
    require_auth( configs.get().manager );
  } else {
    require_auth( get_self() );
  }
  check(token.get_symbol().is_valid(), "invalid symbol");
  white_table wtable(get_self(), get_self().value);
  auto widx = wtable.get_index<"symcode"_n>();
  auto itr = widx.find( token.get_symbol().code().raw() );
  bool white_match = false;
  for( ; itr!=widx.end(); ++itr ) {
    if( itr->token.get_contract() == token.get_contract() &&
        itr->chainName == chain) {
      white_match = true;
      break;
    }
  }
  if( add ) {
    check( !white_match, "can't add "+token.get_symbol().code().to_string()+", already on whitelist." );
    wtable.emplace(get_self(), [&]( auto& s ) {
      s.id = wtable.available_primary_key();
      s.chainName = chain;
      s.token = token;
    });
  } else {
    check( white_match, "can't delete "+token.get_symbol().code().to_string()+", not on whitelist." );
    wtable.erase( *itr );
  }
}

string tokensmaster::json_schema() {
  return 
  R"--({
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$id": "https://github.com/chuck-h/seeds-smart-contracts/blob/feature/master_token_list/assets/token_schema.json",
    "title": "token metadata",
    "description": "This document holds descriptive metadata (e.g. logo image) for cryptotokens used in the Seeds ecosystem",
    "type": "object",
    "properties": {
        "chain": {
            "description": "blockchain name",
            "type": "string",
            "enum": ["Telos","EOS","Wax"]
        },
        "symbol": {
            "description": "symbol ('ticker')",
            "type": "string",
            "minLength":1,
            "maxlength":7
        },
        "account": {
            "description": "account holding token contract",
            "type": "string",
            "minLength":3,
            "maxlength":13
        },
        "name": {
            "description": "short name of the token",
            "type": "string",
            "minLength":3,
            "maxlength":32
        },
        "logo": {
            "description": "url pointing to lo-res logo image",
            "type": "string",
            "minLength":3,
            "maxlength":128
        },
        "logo_lg": {
            "description": "url pointing to hi-res logo image",
            "type": "string",
            "minLength":3,
            "maxlength":128
        },
        "bg_image": {
            "description": "url pointing to card background image",
            "type": "string",
            "minLength":3,
            "maxlength":128
        },
        "subtitle": {
            "description": "wallet balance subtitle",
            "type": "string",
            "minLength":3,
            "maxlength":32
        },
        "precision": {
            "description": "decimal precision for display",
            "type": "integer",
            "minimum":0,
            "maximum":10
        },
        "web_link": {
            "description": "url to webpage with info about token host project",
            "type": "string",
            "minLength":3,
            "maxlength":128
        },
        "contact": {
            "description": "human contact",
            "type": "object",
            "properties": {
                "name": {
                    "description": "name of person",
                    "type": "string",
                    "maxLength": 64
                },
                "email": {
                    "description": "email",
                    "type": "string",
                    "maxLength": 64
                },
                "phone": {
                    "description": "telephone/text number",
                    "type": "string",
                    "maxLength": 32
                }
            }
        }
    },
    "required": [
        "chain",
        "symbol",
        "account",
        "name",
        "logo"
    ],
   "additionalProperties": true
  }
)--";
}


