#include <seeds.rainbows.hpp>
#include <algorithm>
#include <../capi/eosio/action.h>

void rainbows::create( const name&    issuer,
                    const asset&   maximum_supply,
                    const name&    withdrawal_mgr,
                    const name&    withdraw_to,
                    const name&    freeze_mgr,
                    const string&  redeem_locked_until_string,
                    const string&  config_locked_until_string,
                    const string&  membership_symbol,
                    const string&  broker_symbol,
                    const string&  cred_limit_symbol,
                    const string&  pos_limit_symbol )
{
    require_auth( issuer );
    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");
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
    sister_check( membership_symbol, 0);
    sister_check( broker_symbol, 0);
    sister_check( cred_limit_symbol, maximum_supply.symbol.precision());
    sister_check( pos_limit_symbol, maximum_supply.symbol.precision());
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
       cf.withdrawal_mgr = withdrawal_mgr;
       cf.withdraw_to   = withdraw_to;
       cf.freeze_mgr    = freeze_mgr;
       cf.redeem_locked_until = redeem_locked_until;
       cf.config_locked_until = config_locked_until;
       cf.membership = symbol_code( membership_symbol );
       cf.broker = symbol_code( broker_symbol );
       cf.cred_limit = symbol_code( cred_limit_symbol );
       cf.positive_limit = symbol_code( pos_limit_symbol );
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
       .withdrawal_mgr = withdrawal_mgr,
       .withdraw_to   = withdraw_to,
       .freeze_mgr    = freeze_mgr,
       .redeem_locked_until = redeem_locked_until,
       .config_locked_until = config_locked_until,
       .transfers_frozen = false,
       .approved      = false,
       .membership = symbol_code( membership_symbol ),
       .broker = symbol_code( broker_symbol ),
       .cred_limit = symbol_code( cred_limit_symbol ),
       .positive_limit = symbol_code( pos_limit_symbol )
    };
    configtable.set( new_config, issuer );
    displays displaytable( get_self(), sym.code().raw() );
    currency_display new_display{ "" };
    displaytable.set( new_display, issuer );
}

void rainbows::sister_check(const string& sym_name, uint32_t precision) {
    symbol_code sym = symbol_code( sym_name );
    if( sym != symbol_code(0) ) {
       stats statstable_s( get_self(), sym.raw() );
       auto s = statstable_s.get( sym.raw(), (sym_name+" token does not exist").c_str() );
       check( s.max_supply.symbol.precision()==precision, sym_name+" token precision must be "+std::to_string(precision) );
       configs configtable( get_self(), sym.raw() );
       auto cf = configtable.get();
       check( cf.transfers_frozen, sym_name+" token must be frozen" );
    }
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
                      const uint32_t& reserve_fraction,
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
    check( stake_sym.code().raw() != sym_code_raw || stake_token_contract != get_self(),
           "cannot stake own token");
    accounts accountstable( stake_token_contract, st.issuer.value );
    const auto stake_bal = accountstable.find( stake_sym.code().raw() );
    check( stake_bal != accountstable.end(), "issuer must have a stake token balance");
    check( stake_bal->balance.symbol == stake_sym, "mismatched stake token precision" );
    check( is_account( stake_to ), "stake_to account does not exist");
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
       s.reserve_fraction     = reserve_fraction;
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
    check( quantity.amount >= 0, "must issue zero or positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    stake_all( st.issuer, quantity );
    add_balance( st.issuer, quantity, st.issuer, cf.positive_limit );
}

void rainbows::stake_one( const stake_stats& sk, const name& owner, const asset& quantity ) {
    if( sk.stake_per_bucket.amount > 0 ) { // TBD: use stake ratio = 0 as placeholder for proportional?
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
    // get balance in escrow
    auto stake_in_escrow = get_balance( sk.stake_token_contract, sk.stake_to, sk.stake_per_bucket.symbol.code() );
    // stake proportion = (qty being unstaked)/(token supply)
    //  TODO: consider whether negative balances (mutual credit) should count as supply for this calculation
    uint64_t sym_code_raw = sk.token_bucket.symbol.code().raw();
    stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "unstake: no symbol" );
    check( st.supply.amount > 0, "no supply to unstake" );
    int64_t proportional_amount = (int64_t)((int128_t)stake_in_escrow.amount*quantity.amount/st.supply.amount);
    asset stake_quantity = sk.stake_per_bucket;
    string memo;
    if( sk.proportional) {
       stake_quantity.amount = proportional_amount;
       memo = "proportional ";
    } else {
       stake_quantity.amount = (int64_t)((int128_t)quantity.amount*sk.stake_per_bucket.amount/sk.token_bucket.amount);
       // check whether this redemption would put escrow below reserve fraction
       auto stake_remaining = stake_in_escrow.amount - stake_quantity.amount;
       auto supply_remaining = st.supply.amount - quantity.amount;
       auto escrow_needed = (int64_t)((int128_t)supply_remaining*sk.reserve_fraction*sk.stake_per_bucket.amount/
                       (100*sk.token_bucket.amount));
       if( escrow_needed > stake_remaining ) {
          check( false, "can't unstake, escrow underfunded in " +
                 sk.stake_per_bucket.symbol.code().to_string() +
                 //std::to_string(escrow_needed) +":"+ std::to_string(stake_remaining) +
                 " (" + std::to_string(sk.reserve_fraction) + "% reserve)" );
       }
    }
    memo += "rainbow unstake";
    if( stake_quantity.amount > 0 ) {
       action(
          permission_level{sk.stake_to,"active"_n},
          sk.stake_token_contract,
          "transfer"_n,
          std::make_tuple(sk.stake_to,
                          owner,
                          stake_quantity,
                          memo)
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

    unstake_all( owner, quantity );

    sub_balance( owner, quantity, symbol_code(0) );
    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });


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

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount >= 0, "must transfer zero or positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    bool withdrawing = has_auth( cf.withdrawal_mgr ) && to == cf.withdraw_to;
    if (!withdrawing ) {
       require_auth( from );
       if( from != st.issuer ) {
          check( !cf.transfers_frozen, "transfers are frozen");
       }
       if( cf.membership) {
          accounts to_acnts( get_self(), to.value );
          auto to_mbr = to_acnts.find( cf.membership.raw() );
          check( to_mbr != to_acnts.end() && to_mbr->balance.amount >0, "to account must have membership");
          accounts from_acnts( get_self(), from.value );
          auto from_mbr = from_acnts.find( cf.membership.raw() );
          check( from_mbr != from_acnts.end() && from_mbr->balance.amount > 0, "from account must have membership");
          bool vis_to_vis = to_mbr->balance.amount == VISITOR && from_mbr->balance.amount == VISITOR;
          check( !vis_to_vis, "cannot transfer visitor to visitor");
       }
    }

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity, cf.cred_limit );
    add_balance( to, quantity, payer, cf.positive_limit );

    stats statstable2( get_self(), sym_code_raw ); // use updated statstable for supply value
    const auto& st2 = statstable2.get( sym_code_raw );
    check( st2.max_supply.amount >= st2.supply.amount, "new credit exceeds available supply");

}

void rainbows::sub_balance( const name& owner, const asset& value, const symbol_code& limit_symbol ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   int64_t limit = 0;
   if( limit_symbol != symbol_code(0) ) {
      auto cred = from_acnts.find( limit_symbol.raw() );
      if( cred != from_acnts.end() ) {
         const auto& lim = *cred;
         check( lim.balance.symbol.precision() == value.symbol.precision(), "limit precision mismatch" );
         limit = lim.balance.amount;
      }
   }
   int64_t new_balance = from.balance.amount - value.amount;
   check( new_balance + limit >= 0, "overdrawn balance" );
   int64_t credit_increase = std::min( from.balance.amount, 0LL ) - std::min( new_balance, 0LL );
   from_acnts.modify( from, same_payer, [&]( auto& a ) {
         a.balance.amount = new_balance;
      });
   stats statstable( get_self(), value.symbol.code().raw() );
   const auto& st = statstable.get( value.symbol.code().raw() );
   statstable.modify( st, same_payer, [&]( auto& s ) {
      s.supply.amount += credit_increase;
   });
   
}

void rainbows::add_balance( const name& owner, const asset& value, const name& ram_payer, const symbol_code& limit_symbol )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   int64_t limit = value.max_amount;
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
      int64_t new_balance = to->balance.amount + value.amount;
      check( limit >= new_balance, "transfer exceeds receiver positive limit" );
      int64_t credit_increase = std::min( to->balance.amount, 0LL ) - std::min( new_balance, 0LL );
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance.amount = new_balance;
      });
      stats statstable( get_self(), value.symbol.code().raw() );
      const auto& st = statstable.get( value.symbol.code().raw() );
      statstable.modify( st, same_payer, [&]( auto& s ) {
         s.supply.amount += credit_increase;
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
   require_auth( st.issuer );
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
   if( !has_auth( st.issuer ) ) {
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

const asset rainbows::null_asset = asset();
