#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <abieos_numeric.hpp>
#include <contracts.hpp>
#include <tables.hpp>
#include <utils.hpp>
#include <tables/config_table.hpp>

using namespace eosio;
using std::string;
using std::vector;
using std::make_tuple;
using abieos::keystring_authority;
using abieos::authority;

CONTRACT onboarding : public contract {
  public:
    using contract::contract;
    onboarding(name receiver, name code, datastream<const char*> ds)
      : contract(receiver, code, ds),
        sponsors(receiver, receiver.value),
        referrers(receiver, receiver.value),
        campaigns(receiver, receiver.value),
        campinvites(receiver, receiver.value),
        users(contracts::accounts, contracts::accounts.value),
        config(contracts::settings, contracts::settings.value)
        {}

    ACTION reset();
    ACTION deposit(name from, name to, asset quantity, string memo);
    ACTION invite(name sponsor, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash);
    ACTION invitefor(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash);
    ACTION accept(name account, checksum256 invite_secret, string publicKey);
    ACTION acceptnew(name account, checksum256 invite_secret, string publicKey, string fullname);
    ACTION acceptexist(name account, checksum256 invite_secret, string publicKey);
    ACTION onboardorg(name sponsor, name account, string fullname, string publicKey);
    ACTION createbio(name sponsor, name bioregion, string publicKey);

    ACTION cancel(name sponsor, checksum256 invite_hash);

    ACTION cleanup(uint64_t start_id, uint64_t max_id, uint64_t batch_size);

    ACTION createcampg(name origin_account, name owner, asset max_amount_per_invite, asset planted, name reward_owner, asset reward, asset total_amount, uint64_t proposal_id);
    ACTION campinvite(uint64_t id, name authorizing_account, asset planted, asset quantity, checksum256 invite_hash);
    ACTION addauthorized(uint64_t id, name account);
    ACTION remauthorized(uint64_t id, name account);
    ACTION returnfunds(uint64_t id);
    ACTION rtrnfundsaux(uint64_t campaign_id);

  private:
    symbol seeds_symbol = symbol("SEEDS", 4);
    symbol network_symbol = symbol("TLOS", 4);
    uint64_t sow_amount = 50000;

    const name private_campaign = "private"_n;
    const name invite_campaign = "invite"_n;

    void create_account(name account, string publicKey, name domain);
    bool is_seeds_user(name account);
    void add_user(name account, string fullname, name type);
    void transfer_seeds(name account, asset quantity, string memo);
    void plant_seeds(asset quantity);
    void sow_seeds(name account, asset quantity);
    void add_referral(name sponsor, name account);
    void invitevouch(name sponsor, name account);
    void accept_invite(name account, checksum256 invite_secret, string publicKey, string fullname);
    void _invite(name sponsor, name referrer, asset transfer_quantity, asset sow_quantity, checksum256 invite_hash, uint64_t campaign_id);
    void check_user(name account);
    uint64_t config_get(name key);
    void send_campaign_reward(uint64_t campaign_id);
    void send_return_funds_aux(uint64_t campaign_id);


    TABLE invite_table {
      uint64_t invite_id;
      asset transfer_quantity;
      asset sow_quantity;
      name sponsor;
      name account;
      checksum256 invite_hash;
      checksum256 invite_secret;

      uint64_t primary_key()const { return invite_id; }
      uint64_t by_sponsor()const { return sponsor.value; }
      checksum256 by_hash()const { return invite_hash; }
    };

    TABLE referrer_table {
      uint64_t invite_id;
      name referrer;

      uint64_t primary_key()const { return invite_id; }
    };

    TABLE sponsor_table {
      name account;
      asset balance;

      uint64_t primary_key() const { return account.value; }
    };

    TABLE campaign_table {
      uint64_t campaign_id;
      name type;
      name origin_account;
      name owner;
      asset max_amount_per_invite;
      asset planted;
      name reward_owner;
      asset reward;
      std::vector<name> authorized_accounts;
      asset total_amount;
      asset remaining_amount;

      uint64_t primary_key() const { return campaign_id; }
      uint64_t by_type() const { return type.value; }
      uint64_t by_origin_account() const { return origin_account.value; }
      uint64_t by_owner() const { return owner.value; }
    };

    TABLE campaign_invite_table {
      uint64_t invite_id;
      uint64_t campaign_id;

      uint64_t primary_key() const { return invite_id; }
      uint64_t by_campaign() const { return campaign_id; }
      uint128_t by_invite_campaign() const { return (uint128_t(invite_id) << 64) + campaign_id; }
      uint128_t by_campaign_invite() const { return (uint128_t(campaign_id) << 64) + invite_id; }
    };

    DEFINE_CONFIG_TABLE
    DEFINE_CONFIG_TABLE_MULTI_INDEX

    typedef multi_index<"invites"_n, invite_table,
      indexed_by<"byhash"_n,
      const_mem_fun<invite_table, checksum256, &invite_table::by_hash>>,
      indexed_by<"bysponsor"_n,
      const_mem_fun<invite_table, uint64_t, &invite_table::by_sponsor>>
    > invite_tables;

    typedef multi_index<"sponsors"_n, sponsor_table> sponsor_tables;
    typedef multi_index<"referrers"_n, referrer_table> referrer_tables;

    typedef eosio::multi_index<"users"_n, tables::user_table,
      indexed_by<"byreputation"_n,
      const_mem_fun<tables::user_table, uint64_t, &tables::user_table::by_reputation>>
    > user_tables;

    typedef eosio::multi_index<"campaigns"_n, campaign_table,
      indexed_by<"bytype"_n,
      const_mem_fun<campaign_table, uint64_t, &campaign_table::by_type>>,
      indexed_by<"byoriginacct"_n,
      const_mem_fun<campaign_table, uint64_t, &campaign_table::by_origin_account>>,
      indexed_by<"byowner"_n,
      const_mem_fun<campaign_table, uint64_t, &campaign_table::by_owner>>
    > campaign_tables;

    typedef eosio::multi_index<"campinvites"_n, campaign_invite_table,
      indexed_by<"bycampaign"_n,
      const_mem_fun<campaign_invite_table, uint64_t, &campaign_invite_table::by_campaign>>,
      indexed_by<"byinvitecamp"_n,
      const_mem_fun<campaign_invite_table, uint128_t, &campaign_invite_table::by_invite_campaign>>,
      indexed_by<"bycampinvite"_n,
      const_mem_fun<campaign_invite_table, uint128_t, &campaign_invite_table::by_campaign_invite>>
    > campaign_invite_tables;

    sponsor_tables sponsors;
    user_tables users;
    referrer_tables referrers;
    campaign_tables campaigns;
    config_tables config;
    campaign_invite_tables campinvites;

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (action == name("transfer").value && code == contracts::token.value) {
      execute_action<onboarding>(name(receiver), name(code), &onboarding::deposit);
  } else if (code == receiver) {
      switch (action) {
      EOSIO_DISPATCH_HELPER(onboarding, (reset)(invite)(invitefor)(accept)(onboardorg)(createbio)(acceptnew)(acceptexist)(cancel)(cleanup)
      (createcampg)(campinvite)(addauthorized)(remauthorized)(returnfunds)(rtrnfundsaux)
      )
      }
  }
}
