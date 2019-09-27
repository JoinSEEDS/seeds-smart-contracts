#include <seeds.proposals.hpp>

void proposals::reset() {
  require_auth(_self);

  auto pitr = props.begin();

  while (pitr != props.end()) {
      pitr = props.erase(pitr);
  }
}

void proposals::onperiod() {
    require_auth(_self);

    auto pitr = props.begin();
    auto vitr = voice.begin();

    while (pitr != props.end()) {
        if (pitr->favour > pitr->against) {
            withdraw(pitr->recipient, pitr->quantity);
            withdraw(pitr->recipient, pitr->staked);

            props.modify(pitr, _self, [&](auto& proposal) {
                proposal.executed = true;
                proposal.total = 0;
                proposal.favour = 0;
                proposal.against = 0;
                proposal.staked = asset(0, seeds_symbol);
                proposal.status = name("passed");
            });
        } else {
          props.modify(pitr, _self, [&](auto& proposal) {
              proposal.executed = false;
              proposal.total = 0;
              proposal.favour = 0;
              proposal.against = 0;
              proposal.staked = asset(0, seeds_symbol);
              proposal.status = name("rejected");
          });
        }

        pitr++;
    }

    while (vitr != voice.end()) {
        voice.modify(vitr, _self, [&](auto& voice) {
            voice.balance = 1000;
        });

        vitr++;
    }

    transaction trx{};
    trx.actions.emplace_back(
      permission_level(_self, "active"_n),
      _self,
      "onperiod"_n,
      std::make_tuple()
    );
    trx.delay_sec = 2548800;
    trx.send(now(), _self);
}

void proposals::create(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url) {
  require_auth(creator);
  // check_user(creator);
  // check_user(recipient);
  check_asset(quantity);

  uint64_t lastId = 0;
  if (props.begin() != props.end()) {
    auto pitr = props.end();
    pitr--;
    lastId = pitr->id;
  }

  props.emplace(_self, [&](auto& proposal) {
      proposal.id = lastId + 1;
      proposal.creator = creator;
      proposal.recipient = recipient;
      proposal.quantity = quantity;
      proposal.staked = asset(0, seeds_symbol);
      proposal.executed = false;
      proposal.total = 0;
      proposal.favour = 0;
      proposal.against = 0;
      proposal.title = title;
      proposal.summary = summary;
      proposal.description = description;
      proposal.image = image;
      proposal.url = url;
      proposal.creation_date = now();
      proposal.status = name("open");
  });
}

void proposals::update(uint64_t id, string title, string summary, string description, string image, string url) {
  auto pitr = props.find(id);

  check(pitr != props.end(), "no proposal");
  require_auth(pitr->creator);

  props.modify(pitr, _self, [&](auto& proposal) {
    proposal.title = title;
    proposal.summary = summary;
    proposal.description = description;
    proposal.image = image;
    proposal.url = url;
  });
}

void proposals::stake(name from, name to, asset quantity, string memo) {
    if (to == _self) {
        check_user(from);
        check_asset(quantity);

        uint64_t id = std::stoi(memo);

        auto pitr = props.find(id);
        check(pitr != props.end(), "no proposal");

        props.modify(pitr, _self, [&](auto& proposal) {
            proposal.staked += quantity;
        });
    }
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
    proposal.total += amount;
    proposal.favour += amount;
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
        proposal.total += amount;
        proposal.against += amount;
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

void proposals::deposit(asset quantity)
{
  check_asset(quantity);

  auto token_account = config.find(name("tokenaccnt").value)->value;
  auto bank_account = config.find(name("bankaccnt").value)->value;

  token::transfer_action action{name(token_account), {_self, "active"_n}};
  action.send(_self, name(bank_account), quantity, "");
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
