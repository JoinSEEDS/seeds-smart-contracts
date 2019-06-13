#include <seeds.proposals.hpp>

void proposals::reset() {
  require_auth(_self);

  auto pitr = props.begin();

  while (pitr != props.end()) {
      votes_tables votes(_self, pitr->id);
      auto vitr = votes.begin();

      while (vitr != votes.end()) {
          vitr = votes.erase(vitr);
      }

      pitr = props.erase(pitr);
  }

  props.emplace(_self, [&](auto& proposal) {
      proposal.id = 0;
      proposal.creator = _self;
      proposal.recipient = _self;
      proposal.quantity = asset(0, seeds_symbol);
      proposal.memo = "genesis proposal";
      proposal.executed = true;
      proposal.votes = 0;
  });
}

void proposals::create(name creator, name recipient, asset quantity, string memo) {
  require_auth(creator);
  check_user(creator);
  check_user(recipient);
  check_asset(quantity);

  auto pitr = props.end();
  pitr--;
  uint64_t lastId = pitr->id;

  props.emplace(_self, [&](auto& proposal) {
      proposal.id = lastId + 1;
      proposal.creator = creator;
      proposal.recipient = recipient;
      proposal.quantity = quantity;
      proposal.memo = memo;
      proposal.executed = false;
      proposal.votes = 0;
  });
}

void proposals::vote(name voter, uint64_t id) {
  require_auth(voter);
  check_user(voter);

  auto pitr = props.find(id);
  check(pitr != props.end(), "no proposal");
  check(pitr->executed == false, "already executed");

  votes_tables votes(_self, pitr->id);
  auto vitr = votes.find(voter.value);
  check(vitr == votes.end(), "already voted");

  props.modify(pitr, voter, [&](auto& proposal) {
     proposal.votes += 1;
  });

  votes.emplace(voter, [&](auto& vote) {
      vote.account = voter;
  });
}

void proposals::execute(uint64_t id) {
  require_auth(_self);

  auto pitr = props.find(id);
  check(pitr != props.end(), "no proposal");
  check(pitr->executed == false, "already executed");

  auto quota = config.find(name("propsquota").value)->value;
  uint64_t totalUsers = std::distance(users.cbegin(), users.cend());

  check(pitr->votes * 100 / totalUsers > quota, "not enough votes");

  props.modify(pitr, _self, [&](auto& proposal) {
      proposal.executed = true;
  });

  withdraw(pitr->recipient, pitr->quantity);
}

void proposals::withdraw(name beneficiary, asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;

  token::transfer_action action{name(token_account), {name(bank_account), "active"_n}};
  action.send(name(bank_account), beneficiary, quantity, "");
}

void proposals::check_asset(asset quantity)
{
  check(quantity.is_valid(), "invalid asset");
  check(quantity.symbol == seeds_symbol, "invalid asset");
}

void proposals::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
}
