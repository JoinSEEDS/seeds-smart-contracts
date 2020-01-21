#include <seeds.proposals.hpp>
#include <eosio/system.hpp>

void proposals::reset() {
  require_auth(_self);

  auto pitr = props.begin();

  while (pitr != props.end()) {
    votes_tables votes(get_self(), pitr->id);

    auto voteitr = votes.begin();
    while (voteitr != votes.end()) {
      voteitr = votes.erase(voteitr);
    }

    pitr = props.erase(pitr);
  }

  auto voiceitr = voice.begin();
  while (voiceitr != voice.end()) {
    voiceitr = voice.erase(voiceitr);
  }
}

void proposals::onperiod() {
    require_auth(_self);

    auto pitr = props.begin();
    auto vitr = voice.begin();

    uint64_t min_stake = config.find(name("propminstake").value)->value;
    uint64_t voice_base = config.find(name("propvoice").value)->value;

    while (pitr != props.end()) {
      if (pitr->stage == name("active")) {
        if (pitr->favour > pitr->against) {
            if (pitr->staked >= asset(min_stake, seeds_symbol)) {
              withdraw(pitr->recipient, pitr->quantity, pitr->fund);// TODO limit by amount available
              withdraw(pitr->recipient, pitr->staked, contracts::bank);

              props.modify(pitr, _self, [&](auto& proposal) {
                  proposal.executed = true;
                  proposal.staked = asset(0, seeds_symbol);
                  proposal.status = name("passed");
                  proposal.stage = name("done");
              });
            }
        } else {
          burn(pitr->staked);

          props.modify(pitr, _self, [&](auto& proposal) {
              proposal.executed = false;
              proposal.staked = asset(0, seeds_symbol);
              proposal.status = name("rejected");
              proposal.stage = name("done");
          });
        }
      }

      if (pitr->stage == name("staged")) {
        props.modify(pitr, _self, [&](auto& proposal) {
          proposal.stage = name("active");
        });
      }

      pitr++;
    }

    while (vitr != voice.end()) {
        voice.modify(vitr, _self, [&](auto& voice) {
            voice.balance = voice_base;
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
    trx.send(eosio::current_time_point().sec_since_epoch(), _self);
}

void proposals::create(name creator, name recipient, asset quantity, string title, string summary, string description, string image, string url, name fund) {
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
      proposal.creation_date = eosio::current_time_point().sec_since_epoch();
      proposal.status = name("open");
      proposal.stage = name("staged");
      proposal.fund = fund;
  });

  auto litr = lastprops.find(creator.value);
  if (litr == lastprops.end()) {
    lastprops.emplace(_self, [&](auto& proposal) {
      proposal.account = creator;
      proposal.proposal_id = lastId+1;
    });
  } else {
    lastprops.modify(litr, _self, [&](auto& proposal) {
      proposal.account = creator;
      proposal.proposal_id = lastId+1;
    });
  }
}

void proposals::update(uint64_t id, string title, string summary, string description, string image, string url) {
  auto pitr = props.find(id);

  check(pitr != props.end(), "Proposal not found");
  require_auth(pitr->creator);
  check(pitr->favour == 0, "Prop has favor votes - cannot alter proposal once voting has started");
  check(pitr->against == 0, "Prop has against votes - cannot alter proposal once voting has started");

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
      //check_user(from);
      check_asset(quantity);

      uint64_t id = 0;

      if (memo.empty()) {
        auto litr = lastprops.find(from.value);
        check(litr != lastprops.end(), "no proposals");

        id = litr->proposal_id;
      } else {
        id = std::stoi(memo);
      }

      auto pitr = props.find(id);
      check(pitr != props.end(), "no proposal");

      props.modify(pitr, _self, [&](auto& proposal) {
          proposal.staked += quantity;
      });

      deposit(quantity);
  }
}

void proposals::favour(name voter, uint64_t id, uint64_t amount) {
  require_auth(voter);
  check_citizen(voter);

  auto pitr = props.find(id);
  check(pitr != props.end(), "no proposal");
  check(pitr->executed == false, "already executed");

  uint64_t min_stake = config.find(name("propminstake").value)->value;
  check(pitr->staked >= asset(min_stake, seeds_symbol), "not enough stake");
  check(pitr->stage == name("active"), "not active stage");

  auto vitr = voice.find(voter.value);
  check(vitr != voice.end(), "User does not have voice");
  check(vitr->balance >= amount, "voice balance exceeded");

  votes_tables votes(get_self(), id);
  auto voteitr = votes.find(voter.value);
  check(voteitr == votes.end(), "only one vote");

  props.modify(pitr, voter, [&](auto& proposal) {
    proposal.total += amount;
    proposal.favour += amount;
  });

  voice.modify(vitr, voter, [&](auto& voice) {
    voice.balance -= amount;
  });

  votes.emplace(_self, [&](auto& vote) {
    vote.account = voter;
    vote.amount = amount;
    vote.favour = true;
    vote.proposal_id = id;
  });
}

void proposals::against(name voter, uint64_t id, uint64_t amount) {
    require_auth(voter);
    check_citizen(voter);

    auto pitr = props.find(id);
    check(pitr != props.end(), "Proposal not found");
    check(pitr->executed == false, "Proposal was already executed");

    uint64_t min_stake = config.find(name("propminstake").value)->value;
    check(pitr->staked >= asset(min_stake, seeds_symbol), "Proposal does not have enough stake and cannot be voted on.");
    check(pitr->stage == name("active"), "not active stage");

    auto vitr = voice.find(voter.value);
    check(vitr != voice.end(), "User does not have voice");
    check(vitr->balance >= amount, "voice balance exceeded");

    votes_tables votes(get_self(), id);
    auto voteitr = votes.find(voter.value);
    check(voteitr == votes.end(), "only one vote");

    props.modify(pitr, voter, [&](auto& proposal) {
        proposal.total += amount;
        proposal.against += amount;
    });

    voice.modify(vitr, voter, [&](auto& voice) {
        voice.balance -= amount;
    });

    votes.emplace(_self, [&](auto& vote) {
      vote.account = voter;
      vote.amount = amount;
      vote.favour = false;
      vote.proposal_id = id;
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

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::transfer_action action{name(token_account), {_self, "active"_n}};
  action.send(_self, name(bank_account), quantity, "");
}

void proposals::withdraw(name beneficiary, asset quantity, name sender)
{
  if (quantity.amount == 0) return;

  check_asset(quantity);

  auto token_account = contracts::token;

  token::transfer_action action{name(token_account), {sender, "active"_n}};
  action.send(sender, beneficiary, quantity, "");
}

void proposals::burn(asset quantity)
{
  check_asset(quantity);

  auto token_account = contracts::token;
  auto bank_account = contracts::bank;

  token::burn_action action{name(token_account), {name(bank_account), "active"_n}};
  action.send(name(bank_account), quantity);
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

void proposals::check_citizen(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), "no user");
  check(uitr->status == name("citizen"), "user is not a citizen");
}
