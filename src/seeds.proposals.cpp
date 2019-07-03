#include <seeds.proposals.hpp>

void proposals::reset() {
  require_auth(_self);

  auto pitr = props.begin();

  while (pitr != props.end()) {
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

void proposals::onperiod() {
    require_auth(_self);

    auto pitr = props.begin();
    auto vitr = voice.begin();

    while (pitr != props.end()) {
        if (pitr->votes > 0) {
            withdraw(pitr->recipient, pitr->quantity);
        }

        props.modify(pitr, _self, [&](auto& proposal) {
            proposal.executed = false;
            proposal.votes = 0;
        });

        pitr++;
    }

    while (vitr != voice.end()) {
        voice.modify(vitr, _self, [&](auto& voice) {
            voice.balance = 0;
        });

        vitr++;
    }
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

void proposals::favour(name voter, uint64_t id, uint64_t amount) {
  require_auth(voter);
  check_user(voter);

  auto pitr = props.find(id);
  check(pitr != props.end(), "no proposal");
  check(pitr->executed == false, "already executed");

  auto vitr = voice.find(voter.value);
  check(vitr != voice.end(), "no user voice");

  props.modify(pitr, voter, [&](auto& proposal) {
    proposal.votes += amount;
  });

  voice.modify(vitr, voter, [&](auto& voice) {
    voice.balance -= amount;
  });
}

void proposals::against(name voter, uint64_t id, uint64_t amount) {
    require_auth(voter);
    check_user(voter);

    auto pitr = props.find(id);
    check(pitr != props.end(), "no proposal");
    check(pitr->executed == false, "already executed");

    auto vitr = voice.find(voter.value);
    check(vitr != voice.end(), "no user voice");

    props.modify(pitr, voter, [&](auto& proposal) {
        proposal.votes -= amount;
    });

    voice.modify(vitr, voter, [&](auto& voice) {
        voice.balance -= amount;
    });
}

void proposals::addvoice(name user, uint64_t amount)
{
    require_auth(_self);

    auto vitr = voice.find(user.value);

    if (vitr == voice.end()) {
        voice.emplace(_self, [&](auto& voice) {
            voice.account = user;
            voice.balance = amount;
        });
    } else {
        voice.modify(vitr, _self, [&](auto& voice) {
            voice.balance += amount;
        });
    }
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
