#include <seeds.policy.hpp>

void policy::create(name account, string uuid, string signature, string policy) {
  policy_tables policies(_self, account.value);

  auto pitr = policies.find(account.value);
  
  if (pitr == policies.end()) {
    policies.emplace(_self, [&](auto& item) {
      item.account = account;
      item.uuid = uuid;
      item.signature = signature;
      item.policy = policy;
    });
  } else {
    policies.modify(pitr, _self, [&](auto& item) {
      item.account = account;
      item.uuid = uuid;
      item.signature = signature;
      item.policy = policy;
    });
  }
}

void policy::update(name account, string uuid, string signature, string policy) {
  policy_tables policies(_self, account.value);

  auto pitr = policies.find(account.value);
  
  if (pitr == policies.end()) {
    policies.emplace(_self, [&](auto& item) {
      item.account = account;
      item.uuid = uuid;
      item.signature = signature;
      item.policy = policy;
    });
  } else {
    policies.modify(pitr, _self, [&](auto& item) {
      item.account = account;
      item.uuid = uuid;
      item.signature = signature;
      item.policy = policy;
    });
  }
}
