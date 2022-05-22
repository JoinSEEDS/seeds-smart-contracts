#include <seeds.pool.hpp>


ACTION pool::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
    accounts acct(get_self(), bitr->account.value);
    auto aitr = acct.begin();
    while (aitr != acct.end()) {
      aitr = acct.erase(aitr);
    }
    bitr = balances.erase(bitr);
  }

  auto sitr = sizes.begin();
  while (sitr != sizes.end()) {
    sitr = sizes.erase(sitr);
  }
}


ACTION pool::ontransfer (name from, name to, asset quantity, string memo) {

  if (get_first_receiver() == contracts::token  &&  // from SEEDS token account
      to  ==  get_self() &&                     // to here
      quantity.symbol == utils::seeds_symbol) {        // SEEDS symbol

    utils::check_asset(quantity);
    check(from == contracts::escrow, "the sender is not an allowed account");

    name account = name(memo);
    check(is_account(account), account.to_string() + " is not an account");

    add_balance(account, quantity, get_self());
    size_change(total_balance_size, quantity.amount);
  }}

void pool::update_pool_token( const name& owner, const asset& quantity, const symbol sym)
{
   accounts acct(get_self(), owner.value);
   const auto& aitr = acct.get(sym.code().raw(), "update_pool_token: symbol not found");
   acct.modify( aitr, same_payer, [&]( auto& a ){
     a.balance.amount = quantity.amount;
   });
}

void pool::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   const auto& bal_to = balances.find( owner.value );
   if( bal_to == balances.end() ) {
      balances.emplace( get_self(), [&]( auto& a ){
        a.account = owner;
        a.balance = value;
      });
      accounts acct(get_self(), owner.value);
      const auto& aitr = acct.find(utils::pool_symbol.raw());
      if( aitr != acct.end() ) {
        update_pool_token( owner, value ); // stale accounts table entry?
      } else {
        acct.emplace( get_self(), [&]( auto& a ){
          a.balance = value;
          a.balance.symbol = utils::pool_symbol;
        });
      }
   } else {
      check(bal_to->balance.symbol == value.symbol, "incorrect token symbol");
      auto new_balance = bal_to->balance;
      new_balance.amount += value.amount;
      balances.modify( bal_to, same_payer, [&]( auto& a ) {
        a.balance.amount = new_balance.amount;
      });
      update_pool_token( owner, new_balance );
   }
}

bool pool::sub_balance( const name& owner, const asset& value )
{
   bool emptied = false;
   const auto& bal_from = balances.find( owner.value );
   check(bal_from->balance.amount >= value.amount, "poolxfr: overdrawn balance" );
   if (bal_from->balance.amount == value.amount) {
      accounts acct(get_self(), owner.value);
      auto aitr = acct.begin();
      while (aitr != acct.end()) {
        aitr = acct.erase(aitr);
      }
      emptied = true;
    } else {
      asset new_balance = bal_from->balance;
      new_balance.amount -= value.amount;
      balances.modify(bal_from, _self, [&](auto & item){
        item.balance = new_balance;
      });
      update_pool_token( owner, new_balance );
    }
    return emptied;
}

ACTION pool::transfer(name from, name to, asset quantity, const string& memo)
{
  check(quantity.symbol == utils::pool_symbol, "poolxfr: unknown token");
  quantity.symbol = utils::seeds_symbol;
  auto& bal_from = balances.get(from.value, "poolxfr: unknown sender");
  require_auth(from);
  check(is_account(to), "poolxfr: " + to.to_string() + " is not an account");
  check( quantity.amount > 0, "poolxfr: must transfer positive quantity" );
  check( memo.size() <= 256, "poolxfr: memo has more than 256 bytes" );
  bool emptied = sub_balance( from, quantity );
  if( emptied ) { balances.erase(bal_from); }
  name payer = get_self(); // TBD: make from acct pay ram, or a SEEDS fee?
  add_balance( to, quantity, payer );
}

ACTION pool::payouts (asset quantity) {

  require_auth(get_self());

  uint64_t batch_size = config_get("batchsize"_n);
  payout(quantity, uint64_t(0), batch_size, uint64_t(0));

}


ACTION pool::payout (asset quantity, uint64_t start, uint64_t chunksize, int64_t old_total_balance) {

  require_auth(get_self());

  auto bitr = start == 0 ? balances.begin() : balances.lower_bound(start);
  uint64_t current = 0;

  int64_t total_balance = old_total_balance == 0 ? int64_t(get_size(total_balance_size)) : old_total_balance;

  if (total_balance <= 0) { return; }
  if (quantity.amount <= 0) { return; }
  if (total_balance < quantity.amount) { return; }

  double total_balance_divisor = double(total_balance);
  string memo("dSeeds pool distribution");

  while (bitr != balances.end() && current < chunksize) {

    double percentage = bitr->balance.amount / total_balance_divisor;
    asset amount_to_payout = asset(std::min(bitr->balance.amount, int64_t(percentage * quantity.amount)), utils::seeds_symbol);
    
    name account = bitr->account;
    send_transfer(account, amount_to_payout, memo);
    size_change(total_balance_size, -1 * amount_to_payout.amount);

    bool emptied = sub_balance( account, amount_to_payout );
    if( emptied ) {
      bitr = balances.erase(bitr);
    } else {
      bitr++;
    }
    current += 8;
  }

  if (bitr != balances.end()) {
    action next_execution(
      permission_level(get_self(), "active"_n),
      get_self(),
      "payout"_n,
      std::make_tuple(asset(quantity.amount, utils::seeds_symbol), bitr->account.value, chunksize, total_balance)
    );

    transaction tx;
    tx.actions.emplace_back(next_execution);
    tx.delay_sec = 1;
    tx.send(bitr->account.value, _self);
  }
}

void pool::send_transfer (const name & to, const asset & quantity, const string & memo) {

  action(
    permission_level(get_self(), "active"_n),
    contracts::token,
    "transfer"_n,
    std::make_tuple(get_self(), to, quantity, memo)
  ).send();

}

