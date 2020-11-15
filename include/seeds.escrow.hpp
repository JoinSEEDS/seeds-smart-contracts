#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT escrow : public contract {
    public:
        using contract::contract;
        escrow(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
        locks(receiver, receiver.value),
        sponsors(receiver, receiver.value)
        {}

        ACTION reset();

        ACTION lock (   const name&         lock_type, 
                        const name&         sponsor, 
                        const name&         beneficiary,
                        const asset&        quantity, 
                        const name&         trigger_event, 
                        const name&         trigger_source,
                        const time_point&   vesting_date, // = current_time_point() + time_point (1000000000),
                        const string&       notes);

        ACTION trigger (const name&     trigger_source,
                        const name&     event_name,
                        const string&   notes);

        ACTION cancellock (const uint64_t& lock_id);

        ACTION claim(name beneficiary);

        ACTION withdraw(name sponsor, asset quantity);

        [[eosio::on_notify("*::transfer")]]
        void ontransfer(name from, name to, asset quantity, string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);

        // events scoped by trigger_source
        TABLE event {
            name        event_name;
            time_point  event_date   = current_block_time().to_time_point();
            string      notes;

            uint64_t    primary_key()   const { return event_name.value; }
            uint64_t    by_date()       const { return event_date.sec_since_epoch(); }        
        };

        typedef eosio::multi_index<"events"_n, event,
            indexed_by<"bydate"_n, const_mem_fun<event, uint64_t, &event::by_date>>
        > event_table;

        // scoped by get_self()
        TABLE token_lock {
            uint64_t    id;
            name        lock_type;  // "event" or "time", or potentially "latest" or "earliest" 
                                    // if we later want both criteria to apply to the same lock

            name        sponsor;
            name        beneficiary;
            asset       quantity;
            name        trigger_event;  // e.g. "golive" 
            name        trigger_source; // the account that has permissions to trigger the event
            time_point  vesting_date;
            string      notes;
        
            time_point  created_date   = current_block_time().to_time_point();
            time_point  updated_date   = current_block_time().to_time_point();

            uint64_t    primary_key()       const { return id; }  
            uint64_t    by_sponsor()        const { return sponsor.value; }
            uint64_t    by_beneficiary()    const { return beneficiary.value; }
            uint64_t    by_created()        const { return created_date.sec_since_epoch(); }
            uint64_t    by_updated()        const { return updated_date.sec_since_epoch(); }
            uint64_t    by_vesting()        const { return vesting_date.sec_since_epoch(); }
            uint64_t    by_event()          const { return trigger_event.value; }
            uint64_t    by_type()           const { return lock_type.value; }
        };

        typedef eosio::multi_index<"locks"_n, token_lock,
            indexed_by<"bysponsor"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_sponsor>>,
            indexed_by<"bybneficiary"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_beneficiary>>,
            indexed_by<"bycreated"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_created>>,
            indexed_by<"byupdated"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_updated>>,
            indexed_by<"byvesting"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_vesting>>,
            indexed_by<"byevent"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_event>>,
            indexed_by<"bytype"_n, const_mem_fun<token_lock, uint64_t, &token_lock::by_type>>
        > token_lock_table;

        // scoped by get_self()
        TABLE sponsors_table {
            name    sponsor;
            asset   locked_balance = asset (0, symbol("SEEDS", 4));
            asset   liquid_balance = asset (0, symbol("SEEDS", 4));
            
            uint64_t primary_key() const { return sponsor.value; }
        };

        typedef eosio::multi_index<"sponsors"_n, sponsors_table> sponsors_tables;
        
        token_lock_table locks;
        sponsors_tables sponsors;

        void check_asset(asset quantity);
        void init_balance(name user);
        void deduct_from_sponsor (name sponsor, asset locked_quantity);
};
