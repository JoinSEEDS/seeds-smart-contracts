#include <seeds.pool.hpp>


ACTION pool::reset () {
  require_auth(get_self());

  auto bitr = balances.begin();
  while (bitr != balances.end()) {
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

    auto bitr = balances.find(account.value);

    if (bitr == balances.end()) {
      balances.emplace(_self, [&](auto & item){
        item.account = account;
        item.balance = quantity;
      });
    } else {
      balances.modify(bitr, _self, [&](auto & item){
        item.balance += quantity;
      });
    }
    
    size_change(total_balance_size, quantity.amount);
  }

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
    
    send_transfer(bitr->account, amount_to_payout, memo);
    size_change(total_balance_size, -1 * amount_to_payout.amount);

    if (bitr->balance == amount_to_payout) {
      bitr = balances.erase(bitr);
    } else {
      balances.modify(bitr, _self, [&](auto & item){
        item.balance -= amount_to_payout;
      });
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

