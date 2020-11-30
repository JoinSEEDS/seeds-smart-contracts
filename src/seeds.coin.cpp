#include <seeds.policy.hpp>

void coin::reset() {
  require_auth(_self);

  auto pitr = balances.begin();
  while (pitr != balances.end()) {
    pitr = balances.erase(pitr);
  }
}

void coin::setnewkey(name account, string newkey) {
  struct authority {
    uint32_t                                           threshold = 0;
    std::vector<eosiosystem::key_weight>               keys;
    std::vector<eosiosystem::permission_level_weight>  accounts;
    std::vector<eosiosystem::wait_weight>              waits;

    EOSLIB_SERIALIZE( authority, (threshold)(keys)(accounts)(waits) )
  };

// Create the authority type for auth argument of updateauth action
authority newauth;
newauth.threshold = threshold;
eosio::permission_level permission(eosio::name("account_name"), eosio::name("account_permission"));
eosiosystem::permission_level_weight accountpermission{permission, weight};
newauth.accounts.emplace_back(accountpermission);

// Send off the action to updateauth
eosio::action(
  eosio::permission_level(get_self(), eosio::name("active")), 
  eosio::name("eosio"), 
  eosio::name("updateauth"), 
  std::tuple(
    eosio::name("account_name"), 
    eosio::name("permission_name"), 
    eosio::name("parent_name"), 
    newauth) ).send();


}


