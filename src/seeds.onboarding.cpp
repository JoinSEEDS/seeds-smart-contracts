#include <seeds.onboarding.hpp>
#include <eosio/transaction.hpp>

typedef std::variant<std::monostate, uint64_t, int64_t, double, name, asset, string, bool, eosio::time_point> VariantValue; 

void onboarding::create_account(name account, string publicKey, name domain)
{
  if (is_account(account))
    return;

  authority auth = keystring_authority(publicKey);

  if (domain == ""_n)
  {
    domain = _self;
  }

  check_paused();

  action(
      permission_level{domain, "owner"_n},
      "eosio"_n, "newaccount"_n,
      make_tuple(domain, account, auth, auth))
      .send();

  action(
      permission_level{_self, "owner"_n},
      "eosio"_n, 
      "buyrambytes"_n,
      make_tuple(_self, account, 2777)) // 2000 RAM is used by Telos free.tf
      .send();

  action(
      permission_level{_self, "owner"_n},
      "eosio"_n, "delegatebw"_n,
      make_tuple(_self, account, asset(5000, network_symbol), asset(5000, network_symbol), 0))
      .send();
}

bool onboarding::is_seeds_user(name account)
{
  auto uitr = users.find(account.value);
  return uitr != users.end();
}

void onboarding::check_is_banned(name account)
{
  DEFINE_BAN_TABLE
  DEFINE_BAN_TABLE_MULTI_INDEX
  ban_tables ban(contracts::accounts, contracts::accounts.value);

  auto bitr = ban.find(account.value);
  check(bitr == ban.end(), "banned user.");
}


void onboarding::add_user(name account, string fullname, name type)
{

  if (is_seeds_user(account))
  {
    return;
  }

  action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "adduser"_n,
      make_tuple(account, fullname, type))
      .send();
}

void onboarding::check_paused() {
    //check(false, "invite contract is paused, check back later"); // REMOVE THIS AGAIN
}

void onboarding::transfer_seeds(name account, asset quantity, string memo)
{

  if (quantity.amount == 0)
  {
    return;
  }

  check_is_banned(account);

  action(
      permission_level{_self, "active"_n},
      contracts::token, "transfer"_n,
      make_tuple(_self, account, quantity, memo))
      .send();
}

void onboarding::plant_seeds(asset quantity)
{
  string memo("");

  action(
      permission_level{_self, "active"_n},
      contracts::token, "transfer"_n,
      make_tuple(_self, contracts::harvest, quantity, memo))
      .send();
}

void onboarding::sow_seeds(name account, asset quantity)
{
  action(
      permission_level{_self, "active"_n},
      contracts::harvest, "sow"_n,
      make_tuple(_self, account, quantity))
      .send();
}

void onboarding::add_referral(name sponsor, name account)
{
  action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "addref"_n,
      make_tuple(sponsor, account))
      .send();
}

void onboarding::invitevouch(name sponsor, name account)
{
  action(
      permission_level{contracts::accounts, "active"_n},
      contracts::accounts, "invitevouch"_n,
      make_tuple(sponsor, account))
      .send();
}

void onboarding::send_campaign_reward(uint64_t campaign_id)
{
  if (campaign_id == 0)
  {
    return;
  }

  auto citr = campaigns.find(campaign_id);
  if (citr == campaigns.end())
  {
    return;
  }

  transfer_seeds(citr->reward_owner, citr->reward, string("campaign reward"));
}

void onboarding::accept_invite(
    name account, checksum256 invite_secret, string publicKey, string fullname, bool is_existing_telos_account, bool is_plant_seeds)
{

  auto _invite_secret = invite_secret.extract_as_byte_array();
  checksum256 invite_hash = sha256((const char *)_invite_secret.data(), _invite_secret.size());

  checksum256 empty_checksum;

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr != invites_byhash.end(), "invite not found ");
  check(iitr->invite_secret == empty_checksum, "already accepted");

  name referrer = iitr->sponsor;
  auto refitr = referrers.find(iitr->invite_id);
  if (refitr != referrers.end())
  {
    referrer = refitr->referrer;
  }


  invites_byhash.modify(iitr, get_self(), [&](auto &invite)
                        {
                          invite.account = account;
                          invite.invite_secret = invite_secret;
                        });

  asset transfer_quantity = iitr->transfer_quantity;
  asset sow_quantity = iitr->sow_quantity;

  bool is_existing_telos_user = is_account(account);
  bool is_existing_seeds_user = is_seeds_user(account);

  if (is_existing_telos_account)
  {
    check(is_existing_telos_user, "telos account does not exist: " + account.to_string());
  }
  else
  {
    check(!is_existing_telos_user, "telos account already exists: " + account.to_string());
  }

  check_is_banned(referrer);

  if (!is_existing_telos_user)
  {
    create_account(account, publicKey, ""_n);
  }

  if (!is_existing_seeds_user)
  {
    add_user(account, fullname, "individual"_n);
    add_referral(referrer, account);
    invitevouch(referrer, account);
  }

  auto ciitr = campinvites.find(iitr->invite_id);
  if (ciitr != campinvites.end())
  {
    send_campaign_reward(ciitr->campaign_id);
  }

  if (is_plant_seeds)
  {
    transfer_seeds(account, transfer_quantity, string("invite seeds"));
    plant_seeds(sow_quantity);
    sow_seeds(account, sow_quantity);
  }
  else
  {
    transfer_seeds(account, transfer_quantity + sow_quantity, string("invite seeds"));
  }
}

ACTION onboarding::onboardorg(name sponsor, name account, string fullname, string publicKey)
{
  require_auth(get_self());

  check_is_banned(sponsor);

  bool is_existing_telos_user = is_account(account);
  bool is_existing_seeds_user = is_seeds_user(account);

  if (!is_existing_telos_user)
  {
    create_account(account, publicKey, ""_n);
  }

  if (!is_existing_seeds_user)
  {
    add_user(account, fullname, "organisation"_n);
    add_referral(sponsor, account);
  }
}

ACTION onboarding::createregion(name sponsor, name region, string publicKey)
{
  require_auth(get_self());

  check_is_banned(sponsor);

  bool is_existing_telos_user = is_account(region);

  if (!is_existing_telos_user)
  {
    create_account(region, publicKey, "rgn"_n);
  }
}

void onboarding::reset()
{
  require_auth(get_self());

  auto sitr = sponsors.begin();
  while (sitr != sponsors.end())
  {
    sitr = sponsors.erase(sitr);
  }

  auto ritr = referrers.begin();
  while (ritr != referrers.end())
  {
    ritr = referrers.erase(ritr);
  }

  invite_tables invites(get_self(), get_self().value);
  auto iitr = invites.begin();
  while (iitr != invites.end())
  {
    iitr = invites.erase(iitr);
  }

  auto citr = campaigns.begin();
  while (citr != campaigns.end())
  {
    citr = campaigns.erase(citr);
  }

  auto ciitr = campinvites.begin();
  while (ciitr != campinvites.end())
  {
    ciitr = campinvites.erase(ciitr);
  }

  timestamp_tables timestamps(get_self(), get_self().value);
  auto titr = timestamps.begin();
  while (titr != timestamps.end())
  {
    titr = timestamps.erase(titr);
  }
}

// memo = "sponsor acctname" makes accountname the sponsor for this transfer
void onboarding::deposit(name from, name to, asset quantity, string memo)
{
  if (get_first_receiver() == contracts::token && // from SEEDS token account
      to == get_self() &&                         // to here
      quantity.symbol == seeds_symbol)
  { // SEEDS symbol

    utils::check_asset(quantity);

    name sponsor = from;

    if (!memo.empty())
    {
      std::size_t found = memo.find(string("sponsor "));
      if (found != std::string::npos)
      {
        string acct_name = memo.substr(8, string::npos);
        sponsor = name(acct_name);
        check(is_account(sponsor), "Beneficiary sponsor account does not exist " + sponsor.to_string());
      }
      else
      {
        check(false, "invalid memo");
      }
    }

    auto sitr = sponsors.find(sponsor.value);

    if (sitr == sponsors.end())
    {
      sponsors.emplace(_self, [&](auto &asponsor)
                       {
                         asponsor.account = sponsor;
                         asponsor.balance = quantity;
                       });
    }
    else
    {
      sponsors.modify(sitr, _self, [&](auto &asponsor)
                      { asponsor.balance += quantity; });
    }
  }
}

void onboarding::invite(name sponsor, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash)
{
  require_auth(sponsor);
  _invite(sponsor, sponsor, transfer_quantity, sow_quantity, invite_hash, 0);
}

void onboarding::invitefor(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash)
{
  require_auth(sponsor);
  _invite(sponsor, referrer, transfer_quantity, sow_quantity, invite_hash, 0);
}

void onboarding::_invite(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash, uint64_t campaign_id)
{

  asset total_quantity = asset(transfer_quantity.amount + sow_quantity.amount, seeds_symbol);

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr == invites_byhash.end(), "invite hash already exist");

  checksum256 empty_checksum;

  uint64_t key = invites.available_primary_key();

  uint64_t min_planted = config_get("inv.min.plnt"_n);
  check(sow_quantity.amount >= min_planted, "the planted amount must be greater or equal than " + std::to_string(min_planted/10000.0));

  if (campaign_id != 0)
  {
    auto citr = campaigns.find(campaign_id);
    check(citr != campaigns.end(), "campaign not found");

    total_quantity += citr->reward;

    check(total_quantity <= citr->max_amount_per_invite, "max amount per invite exceeded");
    check(total_quantity <= citr->remaining_amount, "remaining amount exceeded");

    campaigns.modify(citr, _self, [&](auto &item)
                     { item.remaining_amount -= total_quantity; });

    campinvites.emplace(_self, [&](auto &item)
                        {
                          item.invite_id = key;
                          item.campaign_id = campaign_id;
                        });
  }
  else
  {
    auto sitr = sponsors.find(sponsor.value);
    check(sitr != sponsors.end(), "sponsor not found");
    check(sitr->balance >= total_quantity, "balance less than " + total_quantity.to_string());

    sponsors.modify(sitr, get_self(), [&](auto &sponsor)
                    { sponsor.balance -= total_quantity; });
  }

  invites.emplace(get_self(), [&](auto &invite)
                  {
                    invite.invite_id = key;
                    invite.transfer_quantity = transfer_quantity;
                    invite.sow_quantity = sow_quantity;
                    invite.sponsor = sponsor;
                    invite.account = name("");
                    invite.invite_hash = invite_hash;
                    invite.invite_secret = empty_checksum;
                  });

  if (referrer != sponsor)
  {
    referrers.emplace(get_self(), [&](auto &item)
                      {
                        item.invite_id = key;
                        item.referrer = referrer;
                      });
  }
}

void onboarding::cancel(name sponsor, checksum256 invite_hash)
{
  require_auth(sponsor);
  _cancel(sponsor, invite_hash, true);
}

void onboarding::_cancel(name sponsor, checksum256 invite_hash, bool check_auth)
{

  check_is_banned(sponsor);

  checksum256 empty_checksum;

  invite_tables invites(get_self(), get_self().value);
  auto invites_byhash = invites.get_index<"byhash"_n>();
  auto iitr = invites_byhash.find(invite_hash);
  check(iitr != invites_byhash.end(), "invite not found");
  check(iitr->invite_secret == empty_checksum, "invite already accepted");

  asset total_quantity = asset(iitr->transfer_quantity.amount + iitr->sow_quantity.amount, seeds_symbol);

  auto ciitr = campinvites.find(iitr->invite_id);

  if (ciitr != campinvites.end())
  {
    auto citr = campaigns.find(ciitr->campaign_id);

    if (citr != campaigns.end())
    {

      if (check_auth)
      {
        check(std::binary_search(citr->authorized_accounts.begin(), citr->authorized_accounts.end(), sponsor),
              sponsor.to_string() + " is not authorized in this campaign");
      }

      campaigns.modify(citr, _self, [&](auto &item)
                       { item.remaining_amount += total_quantity + citr->reward; });

      campinvites.erase(ciitr);
    }
    else
    {
      check(iitr->sponsor == sponsor, "not sponsor");
      transfer_seeds(sponsor, total_quantity, "refund for invite");
    }
  }
  else
  {
    check(iitr->sponsor == sponsor, "not sponsor");
    transfer_seeds(sponsor, total_quantity, "refund for invite");
  }

  auto refitr = referrers.find(iitr->invite_id);
  if (refitr != referrers.end())
  {
    referrers.erase(refitr);
  }

  invites_byhash.erase(iitr);
}

// accept invite creating new account - needs to be called with application key
void onboarding::accept(name account, checksum256 invite_secret, string publicKey)
{
  check(is_account(account) == false, "Account already exists " + account.to_string());

  require_auth(get_self());

  accept_invite(account, invite_secret, publicKey, string(""), false, true);
}

// accept invite with creating new account -  needs to be called with application key
// same as accept but with a full name
void onboarding::acceptnew(name account, checksum256 invite_secret, string publicKey, string fullname)
{
  check(is_account(account) == false, "Account already exists " + account.to_string());

  require_auth(get_self());

  accept_invite(account, invite_secret, publicKey, fullname, false, true);
}

// accept invite using existing Telos account - needs to be signed by existing account
// to prove ownership
void onboarding::acceptexist(name account, checksum256 invite_secret)
{

  check(is_account(account) == true, "Account does not exist " + account.to_string());
  check(is_seeds_user(account) == false, "Account already is Seeds user " + account.to_string());

  require_auth(account);

  accept_invite(account, invite_secret, string(""), string(""), true, true);
}

// reward an existing Seeds user
void onboarding::reward(name account, checksum256 invite_secret)
{
  check(is_seeds_user(account) == true, "Account is not a Seeds user " + account.to_string());

  require_auth(account);

  accept_invite(account, invite_secret, string(""), string(""), true, false);
}

void onboarding::chkcleanup()
{
  require_auth(get_self());

  timestamp_tables timestamps(get_self(), get_self().value);
  invite_tables invites(get_self(), get_self().value);

  if (invites.begin() == invites.end())
  {
    return;
  }

  if (timestamps.begin() != timestamps.end())
  {

    auto titr = timestamps.rbegin();
    uint64_t now = eosio::current_time_point().sec_since_epoch();

    // uncomment this line for tests
    // if (titr->timestamp + 2 > now) { return; }
    if (titr->timestamp + (utils::seconds_per_day * 90) > now)
    {
      return;
    }

    uint64_t start_id = 0;
    uint64_t max_id = titr->invite_id - 1;

    if (titr->id > 0)
    {
      auto prev_itr = titr;
      prev_itr++;

      if (prev_itr->invite_id < max_id)
      {
        start_id = prev_itr->invite_id;
      }
    }

    action(
        permission_level(get_self(), "active"_n),
        get_self(),
        "cleanup"_n,
        std::make_tuple(start_id, max_id, config_get("batchsize"_n)))
        .send();
  }

  auto last_invite_itr = invites.rbegin();

  timestamps.emplace(_self, [&](auto &t)
                     {
                       t.id = timestamps.available_primary_key();
                       t.invite_id = last_invite_itr->invite_id;
                       t.timestamp = eosio::current_time_point().sec_since_epoch();
                     });
}

void onboarding::cleanup(uint64_t start_id, uint64_t max_id, uint64_t batch_size)
{
  require_auth(get_self());

  check(batch_size > 0, "batch size must be > 0");
  check(max_id >= start_id, "max must be > start");

  invite_tables invites(get_self(), get_self().value);

  auto iitr = start_id == 0 ? invites.begin() : invites.lower_bound(start_id);

  uint64_t count = 0;

  uint64_t empty_name_value = name("").value;

  while (iitr != invites.end() && count < batch_size && iitr->invite_id <= max_id)
  {
    if (iitr->account.value == empty_name_value)
    {
      name sponsor = iitr->sponsor;
      checksum256 hash = iitr->invite_hash;
      iitr++;
      count += 8;
      _cancel(sponsor, hash, false);
    }
    else
    {
      iitr++;
      count++;
    }
  }
  if (iitr == invites.end() || iitr->invite_id > max_id)
  {
    // Done.
  }
  else
  {
    // recursive call
    uint64_t next_value = iitr->invite_id;
    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "cleanup"_n,
        std::make_tuple(next_value, max_id, batch_size));

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(next_value, _self);
  }
}

void onboarding::check_user(name account)
{
  auto uitr = users.find(account.value);
  check(uitr != users.end(), account.to_string() + "is not a user");
}

uint64_t onboarding::config_get(name key)
{
  auto citr = config.find(key.value);
  if (citr == config.end())
  {
    check(false, ("settings: the " + key.to_string() + " parameter has not been initialized").c_str());
  }
  return citr->value;
}

ACTION onboarding::createcampg(
    name origin_account,
    name owner,
    asset max_amount_per_invite,
    asset planted,
    name reward_owner,
    asset reward,
    asset total_amount,
    uint64_t proposal_id)
{

  require_auth(origin_account);

  check_user(owner);
  check_user(reward_owner);

  utils::check_asset(max_amount_per_invite);
  utils::check_asset(planted);
  utils::check_asset(reward);
  utils::check_asset(total_amount);

  uint64_t min_planted = config_get("inv.min.plnt"_n);
  check(planted.amount >= min_planted, "the planted amount must be greater or equal than " + std::to_string(min_planted));

  uint64_t max_reward = config_get("inv.max.rwrd"_n);
  check(reward.amount <= max_reward, "the reward can not be greater than " + std::to_string(max_reward));

  auto sitr = sponsors.find(origin_account.value);
  check(sitr != sponsors.end(), "origin account " + origin_account.to_string() + " does not have a balance entry");
  check(sitr->balance >= total_amount, "origin account " + origin_account.to_string() + " does not have enough balance");

  sponsors.modify(sitr, _self, [&](auto &item)
                  { item.balance -= total_amount; });

  name type = private_campaign;
  uint64_t key = campaigns.available_primary_key();
  key = key > 0 ? key : 1;

  if (origin_account == contracts::proposals)
  {
    type = invite_campaign;
    action(
        permission_level(contracts::proposals, "active"_n),
        contracts::proposals,
        "addcampaign"_n,
        std::make_tuple(proposal_id, key))
        .send();
  }

  if (origin_account == contracts::dao)
  {
    std::map<std::string, VariantValue> args;
    args.insert(std::make_tuple("campaign_id", key));
    args.insert(std::make_tuple("proposal_id", proposal_id));

    type = invite_campaign;
    action(
      permission_level(contracts::dao, "active"_n),
      contracts::dao,
      "callback"_n,
      std::make_tuple(args)
    ).send();
  }

  campaigns.emplace(_self, [&](auto &item)
                    {
                      item.campaign_id = key;
                      item.type = type;
                      item.origin_account = origin_account;
                      item.owner = owner;
                      item.max_amount_per_invite = max_amount_per_invite;
                      item.planted = planted;
                      item.reward_owner = reward_owner;
                      item.reward = reward;
                      item.authorized_accounts = {owner};
                      item.total_amount = total_amount;
                      item.remaining_amount = total_amount;
                    });
}

ACTION onboarding::campinvite(uint64_t campaign_id, name authorizing_account, asset planted, asset quantity, checksum256 invite_hash)
{

  auto citr = campaigns.find(campaign_id);
  check(citr != campaigns.end(), "campaign not found");

  bool is_authorized = false;
  for (std::size_t i = 0; i < citr->authorized_accounts.size(); i++)
  {
    if (authorizing_account == citr->authorized_accounts[i])
    {
      is_authorized = true;
      break;
    }
  }

  check(is_authorized, authorizing_account.to_string() + " is not authorized to invite in this campaign");

  require_auth(authorizing_account);

  check(planted >= citr->planted, "campaign: ``````````the planted amount must be greater or equal than " + citr->planted.to_string());
  check(quantity.amount >= 0, "quantity should me greater than 0");

  _invite(citr->origin_account, citr->owner, quantity, planted, invite_hash, campaign_id);
}

ACTION onboarding::addauthorized(uint64_t campaign_id, name account)
{

  auto citr = campaigns.find(campaign_id);
  check(citr != campaigns.end(), "campaign not found");

  require_auth(citr->owner);

  if (std::binary_search(citr->authorized_accounts.begin(), citr->authorized_accounts.end(), account))
  {
    return;
  }

  campaigns.modify(citr, _self, [&](auto &item)
                   { item.authorized_accounts.push_back(account); });
}

ACTION onboarding::remauthorized(uint64_t campaign_id, name account)
{

  auto citr = campaigns.find(campaign_id);
  check(citr != campaigns.end(), "campaign not found");

  require_auth(citr->owner);
  check(account != citr->owner, "can not remove owner");

  auto itr = std::lower_bound(citr->authorized_accounts.begin(), citr->authorized_accounts.end(), account);

  if (itr == citr->authorized_accounts.end() || *itr != account)
  {
    return;
  }

  campaigns.modify(citr, _self, [&](auto &item)
                   { item.authorized_accounts.erase(itr); });
}

ACTION onboarding::returnfunds(uint64_t campaign_id)
{

  auto citr = campaigns.find(campaign_id);
  check(citr != campaigns.end(), "campaign not found");

  name owner = citr->owner;
  name origin_account = citr->origin_account;

  if (has_auth(owner))
  {
    require_auth(owner);
  }
  else
  {
    require_auth(origin_account);
  }

  send_return_funds_aux(campaign_id);
}

void onboarding::send_return_funds_aux(uint64_t campaign_id)
{
  action(
      permission_level(get_self(), "active"_n),
      get_self(),
      "rtrnfundsaux"_n,
      std::make_tuple(campaign_id))
      .send();
}

ACTION onboarding::rtrnfundsaux(uint64_t campaign_id)
{
  require_auth(get_self());

  invite_tables invites(get_self(), get_self().value);

  auto citr = campaigns.find(campaign_id);

  auto campinvites_by_campaigns = campinvites.get_index<"bycampaign"_n>();
  auto itr = campinvites_by_campaigns.find(campaign_id);

  uint64_t batch_size = config_get("batchsize"_n);
  uint64_t count = 0;
  asset total_refund = asset(0, seeds_symbol);

  checksum256 empty_checksum;

  while (itr != campinvites_by_campaigns.end() && itr->campaign_id == campaign_id && count < batch_size)
  {
    auto iitr = invites.find(itr->invite_id);
    if (iitr != invites.end() && iitr->invite_secret == empty_checksum)
    {
      total_refund += iitr->transfer_quantity + iitr->sow_quantity + citr->reward;
      invites.erase(iitr);
    }
    itr = campinvites_by_campaigns.erase(itr);
    count++;
  }

  if (itr != campinvites_by_campaigns.end() && itr->campaign_id == campaign_id)
  {

    transfer_seeds(citr->origin_account, total_refund, "return campaign funds");

    action next_execution(
        permission_level{get_self(), "active"_n},
        get_self(),
        "rtrnfundsaux"_n,
        std::make_tuple(campaign_id));

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(campaign_id, _self);
  }
  else
  {
    transfer_seeds(citr->origin_account, total_refund + citr->remaining_amount, "return campaign funds");
    campaigns.erase(citr);
  }
}