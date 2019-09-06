#include <seeds.policy.hpp>

void policy::create(name account, uint64_t uuid, string signature, string policy) {
  policy_tables policies(_self, account.value);

  auto pitr = policies.find(uuid);
  check(pitr == policies.end(), "existing uuid");

  policies.emplace(_self, [&](auto& item) {
    item.account = account;
    item.uuid = uuid;
    item.signature = signature;
    item.policy = policy;
  });
}

void policy::update(name account, uint64_t uuid, string signature, string policy) {
  policy_tables policies(_self, account.value);

  auto pitr = policies.find(uuid);
  check(pitr != policies.end(), "non-existing uuid");

  policies.modify(pitr, _self, [&](auto& item) {
    item.account = account;
    item.uuid = uuid;
    item.signature = signature;
    item.policy = policy;
  });
}
