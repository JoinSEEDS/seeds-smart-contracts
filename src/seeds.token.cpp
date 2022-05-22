/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <../include/seeds.token.hpp>

namespace eosio {

void token::create( const name&   issuer,
                    const asset&  initial_supply )
{
    require_auth( get_self() );

    auto sym = initial_supply.symbol;
    check( sym.is_valid(), "seeds: invalid symbol name" );
    check( initial_supply.is_valid(), "seeds: invalid supply");
    check( initial_supply.amount > 0, "seeds: max-supply must be positive");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "seeds: token with symbol already exists" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = initial_supply.symbol;
       s.initial_supply  = initial_supply;
       s.issuer        = issuer;
    });
}


void token::issue( const name& to, const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "seeds: invalid symbol name" );
    check( memo.size() <= 256, "seeds: memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "seeds: token with symbol does not exist, create token before issue" );
    const auto& st = *existing;
    check( to == st.issuer, "seeds: tokens can only be issued to issuer account" );

    require_auth( st.issuer );
    check( quantity.is_valid(), "seeds: invalid quantity" );
    check( quantity.amount > 0, "seeds: must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "seeds: symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "seeds: invalid symbol name" );
    check( memo.size() <= 256, "seeds: memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "seeds: token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "seeds: invalid quantity" );
    check( quantity.amount > 0, "seeds: must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "seeds: symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::burn( const name& from, const asset& quantity )
{
  require_auth(from);

  auto sym = quantity.symbol;
  check(sym.is_valid(), "seeds: invalid symbol name");

  stats statstable(get_self(), sym.code().raw());
  auto sitr = statstable.find(sym.code().raw());

  sub_balance(from, quantity);

  statstable.modify(sitr, from, [&](auto& stats) {
    stats.supply -= quantity;
  });
}

void token::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "seeds: cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "seeds: to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check_limit_transactions(from);

    // check_limit(from);

    check( quantity.is_valid(), "seeds: invalid quantity" );
    check( quantity.amount > 0, "seeds: must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "seeds: symbol precision mismatch" );
    check( memo.size() <= 256, "seeds: memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
    
    save_transaction(from, to, quantity);

    update_stats( from, to, quantity );
}

void token::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.find( value.symbol.code().raw());
   check( from != from_acnts.end(), "seeds: no balance object found for " + owner.to_string() );
   check( from->balance.amount >= value.amount, "seeds: overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::save_transaction(name from, name to, asset quantity) {
  if (!is_account(contracts::accounts) || !is_account(contracts::history)) {
    // Before our accounts are created, don't record anything
    return;
  }
  
  action(
    permission_level{contracts::history, "active"_n},
    contracts::history, 
    "trxentry"_n,
    std::make_tuple(from, to, quantity)
  ).send();

}

void token::check_limit_transactions(name from) {
  user_tables users(contracts::accounts, contracts::accounts.value);
  config_tables config(contracts::settings, contracts::settings.value);
  balance_tables balances(contracts::harvest, contracts::harvest.value);

  auto bitr = balances.find(from.value);
  auto uitr = users.find(from.value);

  if (uitr != users.end()) {
    uint64_t max_trx = 0;
    auto min_trx = config.get(name("txlimit.min").value, "The txlimit.min parameters has not been initialized yet.");
    if (bitr != balances.end() && bitr -> planted > asset(0, seeds_symbol)) {
      auto mul_trx = config.get(name("txlimit.mul").value, "The txlimit.mul parameters has not been initialized yet.");
      max_trx = (mul_trx.value * (bitr -> planted).amount) / 10000;
    } 
        
    if (min_trx.value > max_trx) {
      max_trx = min_trx.value;
    }

    transaction_tables transactions(get_self(), seeds_symbol.code().raw());
    auto titr = transactions.find(from.value);

    if (titr != transactions.end()) {
      check(max_trx > titr -> outgoing_transactions, "Maximum limit of allowed transactions reached.");
    }
  }
}

void token::check_limit(const name& from) {
  user_tables users(contracts::accounts, contracts::accounts.value);
  auto uitr = users.find(from.value);

  if (uitr == users.end()) {
    return;
  }

  name status = uitr->status;

  uint64_t limit = 10;
  if (status == "resident"_n) {
    limit = 50;
  } else if (status == "citizen"_n) {
    limit = 100;
  }

  transaction_tables transactions(get_self(), seeds_symbol.code().raw());
  auto titr = transactions.find(from.value);
  uint64_t current = titr->outgoing_transactions;

  check(current < limit, "too many outgoing transactions");
}

void token::reset_weekly_aux(uint64_t begin) {

  config_tables config(contracts::settings, contracts::settings.value);

  auto batch_size = config.get(name("batchsize").value, "The batchsize parameter has not been initialized yet.");
  auto sym_code_raw = seeds_symbol.code().raw();
  uint64_t count = 0;

  transaction_tables transactions(get_self(), sym_code_raw);

  auto titr = begin == 0 ? transactions.begin() : transactions.lower_bound(begin);
  while (titr != transactions.end() && count < batch_size.value) {
    transactions.modify(titr, _self, [&](auto& user) {
      user.incoming_transactions = 0;
      user.outgoing_transactions = 0;
      user.total_transactions = 0;
      user.transactions_volume = asset(0, seeds_symbol);
    });
    titr++;
    count++;
  }

  if (titr != transactions.end()) {
    transaction trx{};
    trx.actions.emplace_back(
      permission_level(_self, "active"_n),
      _self,
      "resetwhelper"_n,
      std::make_tuple((titr -> account).value)
    );
    trx.delay_sec = 1;
    trx.send(eosio::current_time_point().sec_since_epoch(), _self);
  }

}

void token::resetweekly() {
  require_auth(get_self());
  reset_weekly_aux((uint64_t)0);
}

void token::resetwhelper (uint64_t begin) {
  require_auth(get_self());
  reset_weekly_aux(begin);
}

void token::update_stats( const name& from, const name& to, const asset& quantity ) {
    user_tables users(contracts::accounts, contracts::accounts.value);

    auto fromuser = users.find(from.value);
    auto touser = users.find(to.value);
    
    if (fromuser == users.end() || touser == users.end()) {
      return;
    }

    auto sym_code_raw = quantity.symbol.code().raw();
    transaction_tables transactions(get_self(), sym_code_raw);

    auto fromitr = transactions.find(from.value);
    auto toitr = transactions.find(to.value);

    if (fromitr == transactions.end()) {
      transactions.emplace(get_self(), [&](auto& user) {
        user.account = from;
        user.transactions_volume = quantity;
        user.total_transactions = 1;
        user.incoming_transactions = 0;
        user.outgoing_transactions = 1;
      });
    } else {
      transactions.modify(fromitr, get_self(), [&](auto& user) {
          user.transactions_volume += quantity;
          user.outgoing_transactions += 1;
          user.total_transactions += 1;
      });
    }

    if (toitr == transactions.end()) {
      transactions.emplace(get_self(), [&](auto& user) {
        user.account = to;
        user.transactions_volume = quantity;
        user.total_transactions = 1;
        user.incoming_transactions = 1;
        user.outgoing_transactions = 0;
      });
    } else {
      transactions.modify(toitr, get_self(), [&](auto& user) {
        user.transactions_volume += quantity;
        user.total_transactions += 1;
        user.incoming_transactions += 1;
      });
    }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );

   auto sym_code_raw = symbol.code().raw();

   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   check( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

void token::updatecirc() {

   require_auth(get_self());

    // total supply
    stats statstable( get_self(), seeds_symbol.code().raw() );
    auto sitr = statstable.find( seeds_symbol.code().raw() );

    uint64_t total = sitr->supply.amount;
    uint64_t result = total;

    std::array<name, 12> system_accounts = {
      "gift.seeds"_n,
      "milest.seeds"_n,
      "hypha.seeds"_n,
      "allies.seeds"_n,
      "refer.seeds"_n,
      "bank.seeds"_n,
      "system.seeds"_n,
      "harvst.seeds"_n,   // planted - although these go into system actually
      "funds.seeds"_n,    // proposals
      "rules.seeds"_n,    // referendums
      "dao.hypha"_n,      // hypha dao escrow contract
      "escrow.seeds"_n
    };

    for(const auto& account : system_accounts) {   // Range-for!
      result -= balance_for(account);
    }

    circulating_supply_table c = circulating.get_or_create(get_self(), circulating_supply_table());
    c.total = total;
    c.circulating = result;
    circulating.set(c, get_self());

    transaction trx{};
    trx.actions.emplace_back(
      permission_level(_self, "active"_n),
      _self,
      "updatecirc"_n,
      std::make_tuple()
    );
    trx.delay_sec = 60 * 60; // TODO use scheduler for this - run every hour
    trx.send(eosio::current_time_point().sec_since_epoch() + 19, _self);
    
}


uint64_t token::balance_for( const name& owner ) {
   accounts from_acnts( get_self(), owner.value );
   const auto& from = from_acnts.find( seeds_symbol.code().raw());
   if (from != from_acnts.end()) {
     return from->balance.amount;
   } else {
     return 0;
   }
}


void token::minthrvst (const name& to, const asset& quantity, const string& memo) {

  require_auth(get_self());

  auto sym = quantity.symbol;
  check( sym.is_valid(), "TESTS: invalid symbol name" );
  check( memo.size() <= 256, "TESTS: memo has more than 256 bytes" );

  // test tokens only
  check( sym == test_symbol, "TEST: invalid symbol" );

  stats statstable( get_self(), sym.code().raw() );
  auto existing = statstable.find( sym.code().raw() );
  check( existing != statstable.end(), "TESTS: token with symbol does not exist, create token before issue" );
  const auto& st = *existing;
  
  // check( to == st.issuer, "TESTS: tokens can only be issued to issuer account" );

  check( quantity.is_valid(), "TESTS: invalid quantity" );
  check( quantity.amount > 0, "TESTS: must issue positive quantity" );

  check( quantity.symbol == st.supply.symbol, "TESTS: symbol precision mismatch" );

  statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply += quantity;
  });

  add_balance( to, quantity, _self );

}


} /// namespace eosio

EOSIO_DISPATCH( eosio::token, (create)(issue)(transfer)(open)(close)(retire)(burn)(resetweekly)(resetwhelper)(updatecirc)(minthrvst) )
  