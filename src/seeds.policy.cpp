#include <seeds.policy.hpp>
#include <eosio/transaction.hpp>

void policy::reset()
{
  require_auth(_self);

  auto pitr = devicepolicy.begin();
  while (pitr != devicepolicy.end())
  {
    pitr = devicepolicy.erase(pitr);
  }

  auto eitr = expiry.begin();
  while (eitr != expiry.end())
  {
    eitr = expiry.erase(eitr);
  }
}

void policy::create(name account, string backend_user_id, string device_id, string signature, string policy)
{
  require_auth(account);

  devicepolicy.emplace(_self, [&](auto &item) {
    item.id = devicepolicy.available_primary_key();
    item.account = account;
    item.backend_user_id = backend_user_id;
    item.device_id = device_id;
    item.signature = signature;
    item.policy = policy;
  });
}

void policy::createexp(name account, string backend_user_id, string device_id, string signature, string policy, uint64_t expiry_seconds)
{
  require_auth(account);

  check(expiry_seconds <= 60 * 60 * 24 * 7, "expiry must be < 1 week");

  uint64_t policy_id = devicepolicy.available_primary_key();

  devicepolicy.emplace(_self, [&](auto &item) {
    item.id = policy_id;
    item.account = account;
    item.backend_user_id = backend_user_id;
    item.device_id = device_id;
    item.signature = signature;
    item.policy = policy;
  });

  uint64_t now = eosio::current_time_point().sec_since_epoch();

  expiry.emplace(_self, [&](auto &item) {
    item.id = policy_id;
    item.created_at = now;
    item.valid_until = now + expiry_seconds;
  });

  action delete_action(
      permission_level{get_self(), "active"_n},
      get_self(),
      "removeexp"_n,
      std::make_tuple(policy_id));

  transaction tx;
  tx.actions.emplace_back(delete_action);
  tx.delay_sec = expiry_seconds;
  tx.send(policy_id, _self);
}

void policy::update(uint64_t id, name account, string backend_user_id, string device_id, string signature, string policy)
{

  auto pitr = devicepolicy.find(id);

  check(pitr != devicepolicy.end(), "policy with id not found: " + std::to_string(id));
  check(pitr->account == account, "account cannot be changed: " + (pitr->account).to_string());

  require_auth(pitr->account);

  devicepolicy.modify(pitr, _self, [&](auto &item) {
    item.backend_user_id = backend_user_id;
    item.device_id = device_id;
    item.signature = signature;
    item.policy = policy;
  });
}

void policy::remove(uint64_t id)
{
  auto pitr = devicepolicy.find(id);

  check(pitr != devicepolicy.end(), "remove: policy with id not found: " + std::to_string(id));

  require_auth(pitr->account);

  remove_aux(id);
}

void policy::removeexp(uint64_t id)
{
  require_auth(get_self());

  remove_aux(id);
}

void policy::remove_aux(uint64_t id)
{

  auto pitr = devicepolicy.find(id);

  if (pitr != devicepolicy.end())
  {
    devicepolicy.erase(pitr);
  }

  auto eitr = expiry.find(id);

  if (eitr != expiry.end())
  {
    expiry.erase(eitr);
  }
}
