#include <seeds.policy.hpp>

void policy::reset() {
  require_auth(_self);

  auto pitr = devicepolicy.begin();
  while (pitr != devicepolicy.end()) {
    pitr = devicepolicy.erase(pitr);
  }
}

void policy::create(name account, string backend_user_id, string device_id, string signature, string policy) {
  require_auth(account);

  devicepolicy.emplace(_self, [&](auto& item) {
    item.id = devicepolicy.available_primary_key();
    item.account = account;
    item.backend_user_id = backend_user_id;
    item.device_id = device_id;
    item.signature = signature;
    item.policy = policy;
  });

}

void policy::update(uint64_t id, name account, string backend_user_id, string device_id, string signature, string policy) {
  require_auth(account);

  auto pitr = devicepolicy.find(id);

  check(pitr != devicepolicy.end(), "policy with id not found: "+std::to_string(id) );
  
  devicepolicy.modify(pitr, _self, [&](auto& item) {
    item.backend_user_id = backend_user_id;
    item.device_id = device_id;
    item.signature = signature;
    item.policy = policy;
  });
  
}

void policy::remove(uint64_t id) {

  auto pitr = devicepolicy.find(id);

  check(pitr != devicepolicy.end(), "remove: policy with id not found: "+std::to_string(id) );
  
  require_auth(pitr -> account);

  devicepolicy.erase(pitr);
  
}

