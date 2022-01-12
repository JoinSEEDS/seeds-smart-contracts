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
  config_table configs(get_self(), get_self().value);
  configs.remove();
}

void tokensmaster::init(string chain, name manager, bool verify) {
  require_auth(_self);
  config_table configs(get_self(), get_self().value);
  check(!configs.exists(), "cannot re-initialize configuration");
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
  if(configs.get().verify) {
    check(chain == configs.get().chain, "mismatched chain name");
    check(is_account(contract), "no contract account");
    stats statstable( contract, symbolcode.raw() );
    const auto& st = statstable.get( symbolcode.raw(), ("no symbol "+symbolcode.to_string()+" in "+contract.to_string()).c_str());
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
  check(has_auth(manager) || has_auth(get_self()), "not authorized");
  check(symbolcode.is_valid(), "invalid symbol");
  token_tables tokentable(get_self(), get_self().value);
  const auto& tt = tokentable.get(id, ("no match for token id "+std::to_string(id)).c_str());
  check(tt.symbolcode == symbolcode, "symbol doesn't match id");
  usecase_table usecasetable(get_self(), get_self().value);
  const auto& uc = usecasetable.find(usecase.value);
  acceptance_table acceptancetable(get_self(), usecase.value);
  const auto& at = acceptancetable.find(id);
  if(accept) {
    check(at == acceptancetable.end(), "token already accepted");
    if(uc == usecasetable.end()) {
      usecasetable.emplace(get_self(), [&]( auto& s ) {
        s.usecase = usecase;
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

