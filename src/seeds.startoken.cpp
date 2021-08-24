/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <../include/seeds.startoken.hpp>

void startoken::create( const name&   issuer,
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


void startoken::issue( const name& to, const asset& quantity, const string& memo )
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

void startoken::retire( const asset& quantity, const string& memo )
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

void startoken::burn( const name& from, const asset& quantity )
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

void startoken::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "stars: cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "stars: to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "stars: invalid quantity" );
    check( quantity.amount > 0, "stars: must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "stars: symbol precision mismatch" );
    check( memo.size() <= 256, "stars: memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
    
    update_stats( from, to, quantity );
}

void startoken::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.find( value.symbol.code().raw());
   check( from != from_acnts.end(), "stars: no balance object found for " + owner.to_string() );
   check( from->balance.amount >= value.amount, "stars: overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void startoken::add_balance( const name& owner, const asset& value, const name& ram_payer )
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

void startoken::update_stats( const name& from, const name& to, const asset& quantity ) {
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

void startoken::open( const name& owner, const symbol& symbol, const name& ram_payer )
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

void startoken::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

EOSIO_DISPATCH( startoken, (create)(issue)(transfer)(open)(close)(retire)(burn) )
