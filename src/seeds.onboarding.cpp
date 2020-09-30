#include <seeds.onboarding.hpp>
#include <eosio/transaction.hpp>

void onboarding::create_account(name account, string publicKey, name domain) {
  if (is_account(account)) return;

  authority auth = keystring_authority(publicKey);

  if (domain == ""_n) {
    domain = _self;
  }

  action(
    permission_level{domain, "owner"_n},
    "eosio"_n, "newaccount"_n,
    make_tuple(domain, account, auth, auth)
  ).send();

  action(
    permission_level{_self, "owner"_n},
    "eosio"_n, "buyram"_n,
    make_tuple(_self, account, asset(21000, network_symbol))
  ).send();

  action(
    permission_level{_self, "owner"_n},
    "eosio"_n, "delegatebw"_n,
    make_tuple(_self, account, asset(5000, network_symbol), asset(20000, network_symbol), 0)
  ).send();
}

bool onboarding::is_seeds_user(name account) {
  auto uitr = users.find(account.value);
  return uitr != users.end();
}

void onboarding::add_user(name account, string fullname, name type) {

  if (is_seeds_user(account)) {
    return;
  }

  action(
    permission_level{contracts::accounts, "active"_n},
    contracts::accounts, "adduser"_n,
    make_tuple(account, fullname, type)
  ).send();
}

void onboarding::transfer_seeds(name account, asset quantity, string memo) {
  
  if (quantity.amount == 0) {
    return;
  }

  action(
    permission_level{_self, "active"_n},
    contracts::token, "transfer"_n,
    make_tuple(_self, account, quantity, memo)
  ).send();
}

void onboarding::plant_seeds(asset quantity) {
  string memo("");

  action(
    permission_level{_self, "active"_n},
    contracts::token, "transfer"_n,
    make_tuple(_self, contracts::harvest, quantity, memo)
  ).send();
}

void onboarding::sow_seeds(name account, asset quantity) {
  action(
    permission_level{_self, "active"_n},
    contracts::harvest, "sow"_n,
    make_tuple(_self, account, quantity)
  ).send();
}

void onboarding::add_referral(name sponsor, name account) {
  action(
    permission_level{contracts::accounts, "active"_n},
    contracts::accounts, "addref"_n,
    make_tuple(sponsor, account)
  ).send();
}

void onboarding::invitevouch(name sponsor, name account) {
  action(
    permission_level{contracts::accounts, "active"_n},
    contracts::accounts, "invitevouch"_n,
    make_tuple(sponsor, account)
  ).send();
}

void onboarding::accept_invite(name account, checksum256 invite_secret, string publicKey, string fullname) {
  require_auth(get_self());

  auto _invite_secret = invite_secret.extract_as_byte_array();
  checksum256 invite_hash = sha256((const char*)_invite_secret.data(), _invite_secret.size());
  
  checksum256 empty_checksum;

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr != invites_byhash.end(), "invite not found ");
  check(iitr->invite_secret == empty_checksum, "already accepted");

  name referrer = iitr->sponsor;
  auto refitr = referrers.find(iitr->invite_id);
  if (refitr != referrers.end()) {
    referrer = refitr->referrer;
  }

  invites_byhash.modify(iitr, get_self(), [&](auto& invite) {
    invite.account = account;
    invite.invite_secret = invite_secret;
  });

  asset transfer_quantity = iitr->transfer_quantity;
  asset sow_quantity = iitr->sow_quantity;

  bool is_existing_telos_user = is_account(account);
  bool is_existing_seeds_user = is_seeds_user(account);

  if (!is_existing_telos_user) {
    create_account(account, publicKey, ""_n);
  }
  
  if (!is_existing_seeds_user) {
    add_user(account, fullname, "individual"_n);
    add_referral(referrer, account);  
    invitevouch(referrer, account);
  }

  transfer_seeds(account, transfer_quantity, string("invite seeds"));
  plant_seeds(sow_quantity);
  sow_seeds(account, sow_quantity);

}

ACTION onboarding::onboardorg(name sponsor, name account, string fullname, string publicKey) {
    require_auth(get_self());

    bool is_existing_telos_user = is_account(account);
    bool is_existing_seeds_user = is_seeds_user(account);

  if (!is_existing_telos_user) {
    create_account(account, publicKey, ""_n);
  }
  
  if (!is_existing_seeds_user) {
    add_user(account, fullname, "organisation"_n);
    add_referral(sponsor, account);  
  }
}

ACTION onboarding::createbio(name sponsor, name bioregion, string publicKey) {
  require_auth(get_self());

  bool is_existing_telos_user = is_account(bioregion);

  if (!is_existing_telos_user) {
    create_account(bioregion, publicKey, "bdc"_n);
  }
  
}

void onboarding::reset() {
  require_auth(get_self());

  auto sitr = sponsors.begin();
  while (sitr != sponsors.end()) {
    sitr = sponsors.erase(sitr);
  }

  auto ritr = referrers.begin();
  while (ritr != referrers.end()) {
    ritr = referrers.erase(ritr);
  }
  
  invite_tables invites (get_self(), get_self().value);
  auto iitr = invites.begin();
  while (iitr != invites.end()) {
    iitr = invites.erase(iitr);
  }
}


// memo = "sponsor acctname" makes accountname the sponsor for this transfer
void onboarding::deposit(name from, name to, asset quantity, string memo) {
if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
        to  ==  get_self() &&                     // to here
        quantity.symbol == seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);

    name sponsor = from;

    if (!memo.empty()) {
      std::size_t found = memo.find(string("sponsor "));
      if (found!=std::string::npos) {
        string acct_name = memo.substr(8,string::npos);
        sponsor = name(acct_name);
        check(is_account(sponsor), "Beneficiary sponsor account does not exist " + sponsor.to_string());
     } else {
        check(false, "invalid memo");
      }
    }

    auto sitr = sponsors.find(sponsor.value);

    if (sitr == sponsors.end()) {
      sponsors.emplace(_self, [&](auto& asponsor) {
        asponsor.account = sponsor;
        asponsor.balance = quantity;
      });
    } else {
      sponsors.modify(sitr, _self, [&](auto& asponsor) {
        asponsor.balance += quantity;
      });
    }
  }
}

void onboarding::invite(name sponsor, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash) {
  _invite(sponsor, sponsor, transfer_quantity, sow_quantity, invite_hash);
}

void onboarding::invitefor(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash) {
  _invite(sponsor, referrer, transfer_quantity, sow_quantity, invite_hash);
}

void onboarding::_invite(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash) {
  require_auth(sponsor);

  asset total_quantity = asset(transfer_quantity.amount + sow_quantity.amount, seeds_symbol);

  auto sitr = sponsors.find(sponsor.value);
  check(sitr != sponsors.end(), "sponsor not found");
  check(sitr->balance >= total_quantity, "balance less than " + total_quantity.to_string());

  sponsors.modify(sitr, get_self(), [&](auto& sponsor) {
    sponsor.balance -= total_quantity;
  });

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr == invites_byhash.end(), "invite hash already exist");

  checksum256 empty_checksum;

  uint64_t key = invites.available_primary_key();

  invites.emplace(get_self(), [&](auto& invite) {
    invite.invite_id = key;
    invite.transfer_quantity = transfer_quantity;
    invite.sow_quantity = sow_quantity;
    invite.sponsor = sponsor;
    invite.account = name("");
    invite.invite_hash = invite_hash;
    invite.invite_secret = empty_checksum;
  });

  if (referrer != sponsor) {
    referrers.emplace(get_self(), [&](auto& item) {
      item.invite_id = key;
      item.referrer = referrer;
    });
  }
}

void onboarding::cancel(name sponsor, checksum256 invite_hash) {
  require_auth(sponsor);

  checksum256 empty_checksum;

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr != invites_byhash.end(), "invite not found");
  check(iitr->invite_secret == empty_checksum, "invite already accepted");

  auto refitr = referrers.find(iitr->invite_id);
  if (refitr != referrers.end()) {
    referrers.erase(refitr);
  }

  asset total_quantity = asset(iitr->transfer_quantity.amount + iitr->sow_quantity.amount, seeds_symbol);

  transfer_seeds(sponsor, total_quantity, "refund for invite");

  invites_byhash.erase(iitr);
}

// accept invite with creating new account
void onboarding::acceptnew(name account, checksum256 invite_secret, string publicKey, string fullname) {
  check(is_account(account) == false, "Account already exists " + account.to_string());

  accept_invite(account, invite_secret, publicKey, fullname);
}

// accept invite using already existing account
void onboarding::acceptexist(name account, checksum256 invite_secret, string publicKey) {
  check(is_account(account) == true, "Account does not exist " + account.to_string());

  accept_invite(account, invite_secret, publicKey, string(""));
}

// accept invite using already existing account or creating new account
void onboarding::accept(name account, checksum256 invite_secret, string publicKey) {
  accept_invite(account, invite_secret, publicKey, string(""));
}

void onboarding::cleanup(uint64_t start_id, uint64_t max_id, uint64_t batch_size) {
  require_auth(get_self());

  check(batch_size > 0, "batch size must be > 0");
  check(max_id >= start_id, "max must be> start");
  
  invite_tables invites(get_self(), get_self().value);

  auto iitr = start_id == 0 ? invites.begin() : invites.lower_bound(start_id);

  uint64_t count = 0;

  uint64_t empty_name_value = name("").value;

  while(iitr != invites.end() && count < batch_size && iitr->invite_id <= max_id) {
    if (iitr->account.value == empty_name_value) {
      asset total_quantity = asset(iitr->transfer_quantity.amount + iitr->sow_quantity.amount, seeds_symbol);
      transfer_seeds(iitr->sponsor, total_quantity, "refund for invite");
      iitr = invites.erase(iitr);
      auto refitr = referrers.find(iitr->invite_id);
      if (refitr != referrers.end()) {
        referrers.erase(refitr);
      }
      count += 4;
    } else {
      iitr++;
      count++;
    }
  }
  if (iitr == invites.end() || iitr->invite_id > max_id) {
    // Done.
  } else {
    // recursive call
    uint64_t next_value = iitr->invite_id;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "cleanup"_n,
        std::make_tuple(next_value, max_id, batch_size)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
    
  }



}
