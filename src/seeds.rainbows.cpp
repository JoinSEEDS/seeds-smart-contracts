#include <seeds.rainbows.hpp>
#include <../capi/eosio/action.h>

void rainbows::create( const name&    issuer,
                    const asset&   maximum_supply,
                    const name&    membership_mgr,
                    const name&    withdrawal_mgr,
                    const name&    withdraw_to,
                    const name&    freeze_mgr,
                    const string&  redeem_locked_until_string,
                    const string&  config_locked_until_string,
                    const string&  cred_limit_symbol,
                    const string&  pos_limit_symbol )
{
    require_auth( issuer );
    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");
    check( is_account( membership_mgr ) || membership_mgr == allowallacct,
        "membership_mgr account does not exist");
    check( is_account( withdrawal_mgr ), "withdrawal_mgr account does not exist");
    check( is_account( withdraw_to ), "withdraw_to account does not exist");
    check( is_account( freeze_mgr ), "freeze_mgr account does not exist");
    time_point redeem_locked_until = current_time_point();
    if( redeem_locked_until_string != "" ) {
       redeem_locked_until = time_point::from_iso_string( redeem_locked_until_string );
       auto days_from_now = (redeem_locked_until.time_since_epoch() -
                             current_time_point().time_since_epoch()).count()/days(1).count();
       check( days_from_now < 100*365 && days_from_now > -10*365, "redeem lock date out of range" );
    }
    time_point config_locked_until = current_time_point();
    if( config_locked_until_string != "" ) {
       config_locked_until = time_point::from_iso_string( config_locked_until_string );
       auto days_from_now = (config_locked_until.time_since_epoch() -
                             current_time_point().time_since_epoch()).count()/days(1).count();
       check( days_from_now < 100*365 && days_from_now > -10*365, "config lock date out of range" );
    }
    symbol_code credlim = symbol_code( cred_limit_symbol );
    if( credlim != symbol_code(0) ) {
       stats statstable_cl( get_self(), credlim.raw() );
       auto cl = statstable_cl.get( credlim.raw(), "credit limit token does not exist" );
       check( cl.max_supply.symbol.precision()==maximum_supply.symbol.precision(), "credit limit token precision does not match" );
    }
    symbol_code poslim = symbol_code( pos_limit_symbol );
    if( poslim != symbol_code(0) ) {
       stats statstable_pl( get_self(), poslim.raw() );
       auto pl = statstable_pl.get( poslim.raw(), "positive limit token does not exist" );
       check( pl.max_supply.symbol.precision()==maximum_supply.symbol.precision(), "credit limit token precision does not match" );
    }
    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    if( existing != statstable.end()) {
       // token exists
       const auto& st = *existing;
       configs configtable( get_self(), sym.code().raw() );
       auto cf = configtable.get();
       check( cf.config_locked_until.time_since_epoch() < current_time_point().time_since_epoch(),
              "token reconfiguration is locked" );
       check( st.issuer == issuer, "mismatched issuer account" );
       if( st.supply.amount != 0 ) {
          check( sym == st.supply.symbol,
                 "cannot change symbol precision with outstanding supply" );
          //TBD: could support precision change by walking accounts table
          check( maximum_supply.amount >= st.supply.amount,
                 "cannot reduce maximum below outstanding supply" );
       }
       statstable.modify (st, issuer, [&]( auto& s ) {
          s.supply.symbol = maximum_supply.symbol;
          s.max_supply    = maximum_supply;
          s.issuer        = issuer;
       });
       cf.membership_mgr = membership_mgr;
       cf.withdrawal_mgr = withdrawal_mgr;
       cf.withdraw_to   = withdraw_to;
       cf.freeze_mgr    = freeze_mgr;
       cf.redeem_locked_until = redeem_locked_until;
       cf.config_locked_until = config_locked_until;
       cf.cred_limit = credlim;
       cf.positive_limit = poslim;
       configtable.set( cf, issuer );
    return;
    }
    // new token
    symbols symboltable( get_self(), get_self().value );
    symboltable.emplace( issuer, [&]( auto& s ) {
       s.symbolcode = sym.code();
    });
    statstable.emplace( issuer, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
    configs configtable( get_self(), sym.code().raw() );
    currency_config new_config{
       .membership_mgr = membership_mgr,
       .withdrawal_mgr = withdrawal_mgr,
       .withdraw_to   = withdraw_to,
       .freeze_mgr    = freeze_mgr,
       .redeem_locked_until = redeem_locked_until,
       .config_locked_until = config_locked_until,
       .transfers_frozen = false,
       .approved      = false,
       .cred_limit = credlim,
       .positive_limit = poslim
    };
    configtable.set( new_config, issuer );
    displays displaytable( get_self(), sym.code().raw() );
    currency_display new_display{ "" };
    displaytable.set( new_display, issuer );
}

void rainbows::approve( const symbol_code& symbolcode, const bool& reject_and_clear )
{
    require_auth( get_self() );
    auto sym_code_raw = symbolcode.raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "token with symbol does not exist" );
    configs configtable( get_self(), sym_code_raw );
    auto cf = configtable.get();
    displays displaytable( get_self(), sym_code_raw );
    if( reject_and_clear ) {
       check( st.supply.amount == 0, "cannot clear with outstanding tokens" );
       stakes stakestable( get_self(), sym_code_raw );
       for( auto itr = stakestable.begin(); itr != stakestable.end(); ) {
          itr = stakestable.erase(itr);
       }
       configtable.remove( );
       displaytable.remove( );
       statstable.erase( statstable.iterator_to(st) );
       symbols symboltable( get_self(), get_self().value );
       const auto& sym = symboltable.get( sym_code_raw );
       symboltable.erase( sym );
    } else {
       cf.approved = true;
       configtable.set (cf, st.issuer );
    }

}

void rainbows::setstake( const asset&    token_bucket,
                      const asset&    stake_per_bucket,
                      const name&     stake_token_contract,
                      const name&     stake_to,
                      const bool&     proportional,
                      const string&   memo )
{
    auto sym_code_raw = token_bucket.symbol.code().raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "token with symbol does not exist" );
    require_auth( st.issuer );
    check( memo.size() <= 256, "memo has more than 256 bytes" );
    auto stake_sym = stake_per_bucket.symbol;
    uint128_t stake_token = (uint128_t)stake_sym.raw()<<64 | stake_token_contract.value;
    check( stake_sym.is_valid(), "invalid stake symbol name" );
    check( stake_per_bucket.is_valid(), "invalid stake");
    check( stake_per_bucket.amount >= 0, "stake per token must be non-negative");
    check( is_account( stake_token_contract ), "stake token contract account does not exist");
    accounts accountstable( stake_token_contract, st.issuer.value );
    const auto stake_bal = accountstable.find( stake_sym.code().raw() );
    check( stake_bal != accountstable.end(), "issuer must have a stake token balance");
    check( stake_bal->balance.symbol == stake_sym, "mismatched stake token precision" );
    if( stake_to != deletestakeacct ) {
       check( is_account( stake_to ), "stake_to account does not exist");
    }
    check( token_bucket.amount > 0, "token bucket must be > 0" );
    configs configtable( get_self(), sym_code_raw );
    const auto& cf = configtable.get();
    check( cf.config_locked_until.time_since_epoch() < current_time_point().time_since_epoch(),
           "token reconfiguration is locked" );
    stakes stakestable( get_self(), sym_code_raw );
    int existing_stake_count = std::distance(stakestable.cbegin(),stakestable.cend());
    check( existing_stake_count <= max_stake_count, "stake count exceeded" );
    const auto& sk = *stakestable.emplace( st.issuer, [&]( auto& s ) {
       s.index                = stakestable.available_primary_key();
       s.token_bucket         = token_bucket;
       s.stake_per_bucket     = stake_per_bucket;
       s.stake_token_contract = stake_token_contract;
       s.stake_to             = stake_to;
       s.proportional         = proportional;
    });
    if( st.supply.amount != 0 ) {
       stake_one( sk, st.issuer, st.supply );
    }

}

void rainbows::deletestake( const uint64_t& stake_index,
                            const symbol_code& symbolcode,
                            const string& memo )
{
    auto sym_code_raw = symbolcode.raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "token with symbol does not exist" );
    stakes stakestable( get_self(), sym_code_raw );
    const auto& sk = stakestable.get( stake_index, "stake index does not exist" );
    configs configtable( get_self(), sym_code_raw );
    const auto& cf = configtable.get();
    check( cf.config_locked_until.time_since_epoch() < current_time_point().time_since_epoch(),
           "token reconfiguration is locked" );
    require_auth( st.issuer );
    if( st.supply.amount != 0 ) {
        unstake_one( sk, st.issuer, st.supply );
    }
    stakestable.erase( sk );
}

void rainbows::setdisplay( const symbol_code&  symbolcode,
                           const string&       json_meta )
{
    auto sym_code_raw = symbolcode.raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "token with symbol does not exist" );
    require_auth( st.issuer );
    displays displaytable( get_self(), sym_code_raw );
    auto dt = displaytable.get();
    check( json_meta.size() <= 2048, "json metadata has more than 2048 bytes" );
    // TODO check json_meta string for safety, parse json, and check name length
    dt.json_meta   = json_meta;
    displaytable.set( dt, st.issuer );
}

void rainbows::issue( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    const auto& st = statstable.get( sym.code().raw(), "token with symbol does not exist, create token before issue" );
    configs configtable( get_self(), sym.code().raw() );
    const auto& cf = configtable.get();
    check( cf.approved, "cannot issue until token is approved" );
    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    stake_all( st.issuer, quantity );
    add_balance( st.issuer, quantity, st.issuer, cf.positive_limit );
}

void rainbows::stake_one( const stake_stats& sk, const name& owner, const asset& quantity ) {
    if( sk.stake_per_bucket.amount > 0 ) {
       asset stake_quantity = sk.stake_per_bucket;
       stake_quantity.amount = (int64_t)((int128_t)quantity.amount*sk.stake_per_bucket.amount/sk.token_bucket.amount);
       action(
          permission_level{owner, "active"_n},
          sk.stake_token_contract,
          "transfer"_n,
          std::make_tuple(owner,
                          sk.stake_to,
                          stake_quantity,
                          std::string("rainbow stake"))
       ).send();
    }
}

void rainbows::stake_all( const name& owner, const asset& quantity ) {
    stakes stakestable( get_self(), quantity.symbol.code().raw() );
    for( auto itr = stakestable.begin(); itr != stakestable.end(); itr++ ) {
       stake_one( *itr, owner, quantity );
    }
}

void rainbows::unstake_one( const stake_stats& sk, const name& owner, const asset& quantity ) {
    check( !sk.proportional, "proportional stake not implemented" );
    if( sk.stake_per_bucket.amount > 0 ) {
       asset stake_quantity = sk.stake_per_bucket;
       stake_quantity.amount = (int64_t)((int128_t)quantity.amount*sk.stake_per_bucket.amount/sk.token_bucket.amount);
       // TODO (a) if proportional, compute unstake amount based on current escrow balance and note in memo string
       //      (b) if not proportional, check that escrow is fully funded
       action(
          permission_level{sk.stake_to,"active"_n},
          sk.stake_token_contract,
          "transfer"_n,
          std::make_tuple(sk.stake_to,
                          owner,
                          stake_quantity,
                          std::string("rainbow unstake"))
       ).send();
    }
}
void rainbows::unstake_all( const name& owner, const asset& quantity ) {
    stakes stakestable( get_self(), quantity.symbol.code().raw() );
    for( auto itr = stakestable.begin(); itr != stakestable.end(); itr++ ) {
       unstake_one( *itr, owner, quantity );
    }
}

void rainbows::retire( const name& owner, const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    const auto& st = statstable.get( sym.code().raw(), "token with symbol does not exist" );
    configs configtable( get_self(), sym.code().raw() );
    const auto& cf = configtable.get();
    if( cf.redeem_locked_until.time_since_epoch() < current_time_point().time_since_epoch() ) {
       check( !cf.transfers_frozen, "transfers are frozen");
    } else {
       check( owner == st.issuer, "bearer redeem is disabled");
    }
    require_auth( owner );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( owner, quantity, symbol_code(0) );
    unstake_all( owner, quantity );
}

void rainbows::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "cannot transfer to self" );
    check( is_account( from ), "from account does not exist");
    check( is_account( to ), "to account does not exist");
    auto sym_code_raw = quantity.symbol.code().raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw );
    configs configtable( get_self(), sym_code_raw );
    const auto& cf = configtable.get();

    bool withdrawing = has_auth( cf.withdrawal_mgr ) && to == cf.withdraw_to;
    if( cf.membership_mgr != allowallacct || withdrawing) {
       accounts to_acnts( get_self(), to.value );
       auto to = to_acnts.find( sym_code_raw );
       check( to != to_acnts.end(), "to account must have membership");
    }

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    if (!withdrawing ) {
       require_auth( from );
       if( from != st.issuer ) {
          check( !cf.transfers_frozen, "transfers are frozen");
       }
       // TBD: implement token.seeds check_limit_transactions(from) ?
    }

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity, cf.cred_limit );
    add_balance( to, quantity, payer, cf.positive_limit );

    // TODO: consider whether tokens transferred from/to accounts in negative balance should be
    //  taken from/returned to the issuer account. This would ensure that there is stake for
    //  all the tokens in circulation.

}

void rainbows::sub_balance( const name& owner, const asset& value, const symbol_code& limit_symbol ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   uint64_t limit = 0;
   if( limit_symbol != symbol_code(0) ) {
      auto cred = from_acnts.find( limit_symbol.raw() );
      if( cred != from_acnts.end() ) {
         const auto& lim = *cred;
         check( lim.balance.symbol.precision() == value.symbol.precision(), "limit precision mismatch" );
         limit = lim.balance.amount;
      }
   }
   check( from.balance.amount + limit >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, same_payer, [&]( auto& a ) {
         a.balance -= value;
      });
}

void rainbows::add_balance( const name& owner, const asset& value, const name& ram_payer, const symbol_code& limit_symbol )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   uint64_t limit = value.max_amount;
   if( limit_symbol != symbol_code(0) ) {
      auto pos = to_acnts.find( limit_symbol.raw() );
      if( pos != to_acnts.end() ) {
         const auto& lim = *pos;
         check( lim.balance.symbol.precision() == value.symbol.precision(), "limit precision mismatch" );
         limit = lim.balance.amount;
      }
   }
   if( to == to_acnts.end() ) {
      check( limit >= value.amount, "transfer exceeds receiver positive limit" );
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      check( limit >= to->balance.amount + value.amount, "transfer exceeds receiver positive limit" );
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void rainbows::open( const name& owner, const symbol_code& symbolcode, const name& ram_payer )
{
   require_auth( ram_payer );

   check( is_account( owner ), "owner account does not exist" );

   auto sym_code_raw = symbolcode.raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   configs configtable( get_self(), sym_code_raw );
   const auto& cf = configtable.get();
   if( cf.membership_mgr != allowallacct) {
      require_auth( cf.membership_mgr );
   }
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, st.supply.symbol};
      });
   }
}

void rainbows::close( const name& owner, const symbol_code& symbolcode )
{
   auto sym_code_raw = symbolcode.raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   configs configtable( get_self(), sym_code_raw );
   const auto& cf = configtable.get();
   if( cf.membership_mgr == allowallacct || !has_auth( cf.membership_mgr ) ) {
      require_auth( owner );
   }
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

void rainbows::freeze( const symbol_code& symbolcode, const bool& freeze, const string& memo )
{
   auto sym_code_raw = symbolcode.raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   configs configtable( get_self(), sym_code_raw );
   auto cf = configtable.get();
   check( memo.size() <= 256, "memo has more than 256 bytes" );
   require_auth( cf.freeze_mgr );
   cf.transfers_frozen = freeze;
   configtable.set (cf, st.issuer );
}

void rainbows::reset( const bool all, const uint32_t limit )
{
  uint32_t counter= 0;
  require_auth2( get_self().value, "active"_n.value );
  auto sym = symboltable.begin();
  while (sym != symboltable.end()) {
    reset_one(sym->symbolcode, all, limit, counter);
    if( counter > limit ) { return; }
    if( all ) {
      sym = symboltable.erase(sym);
    } else {
      ++sym;
    }
  }
}
  
void rainbows::resetacct( const name& account )
{
  require_auth2( get_self().value, "active"_n.value );
    accounts tbl(get_self(),account.value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
}

void rainbows::reset_one( const symbol_code symbolcode, const bool all, const uint32_t limit, uint32_t& counter )
{
     auto scope = symbolcode.raw();
     {
       configs tbl(get_self(),scope);
       tbl.remove();
       if( ++counter > limit ) { goto CountedOut; }
     }
     {
       displays tbl(get_self(),scope);
       tbl.remove();
       if( ++counter > limit ) { goto CountedOut; }
     }
     {
       stakes tbl(get_self(),scope);
       auto itr = tbl.begin();
       while (itr != tbl.end()) {
         itr = tbl.erase(itr);
         if( ++counter > limit ) { goto CountedOut; }
       }
     }
     if( all ) {
       {
         stats tbl(get_self(),scope);
         auto itr = tbl.begin();
         while (itr != tbl.end()) {
           itr = tbl.erase(itr);
           if( ++counter > limit ) { goto CountedOut; }
         }
       }
     }
     CountedOut: return;
}

