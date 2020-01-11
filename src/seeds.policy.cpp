#include <seeds.policy.hpp>

void policy::reset() {
  require_auth(_self);

  /*
  policy_tables_new policies(_self, name("seedsuser444").value);

  auto pitr = policies.begin();

  while (pitr != policies.end()) {
    pitr = policies.erase(pitr);
  }
  */
}

void policy::create(name account, string backend_user_id, string device_id, string signature, string policy) {
  require_auth(account);

  policy_tables_new policies(_self, account.value);

  auto pitr = policies.find(account.value);

  if (pitr == policies.end()) {
    policies.emplace(_self, [&](auto& item) {
      item.account = account;
      item.backend_user_id = backend_user_id;
      item.device_id = device_id;
      item.signature = signature;
      item.policy = policy;
    });
  } else {
    policies.modify(pitr, _self, [&](auto& item) {
      item.account = account;
      item.backend_user_id = backend_user_id;
      item.device_id = device_id;
      item.signature = signature;
      item.policy = policy;
    });
  }
}

void policy::update(name account, string backend_user_id, string device_id, string signature, string policy) {
  require_auth(account);

  policy_tables_new policies(_self, account.value);

  auto pitr = policies.find(account.value);

  if (pitr == policies.end()) {
    policies.emplace(_self, [&](auto& item) {
      item.account = account;
      item.backend_user_id = backend_user_id;
      item.device_id = device_id;
      item.signature = signature;
      item.policy = policy;
    });
  } else {
    policies.modify(pitr, _self, [&](auto& item) {
      item.account = account;
      item.backend_user_id = backend_user_id;
      item.device_id = device_id;
      item.signature = signature;
      item.policy = policy;
    });
  }
}
