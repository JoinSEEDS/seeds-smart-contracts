#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>
#include <seeds.token.hpp>
#include <contracts.hpp>

using namespace eosio;
using std::string;

CONTRACT vstandescrow : public contract {
    public:
        using contract::contract;
        vstandescrow(name receiver, name code, datastream<const char*> ds)
        : contract(receiver, code, ds),
        locks(receiver, receiver.value),
        sponsors(receiver, receiver.value)
        {}

        ACTION reset();

        // ACTION create(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date);

        // ACTION createescrow(name sponsor_bucket, name beneficiary_org, asset quantity, uint64_t vesting_date);

        ACTION lock (   const name&         lock_type, 
                        const name&         sponsor, 
                        const name&         beneficiary,
                        const asset&        quantity, 
                        const name&         trigger_event, 
                        const time_point&   vesting_date,
                        const string&       notes);

        ACTION trigger (const name&     sponsor,
                        const name&     event_name,
                        const string&   notes);

        // ACTION cancel(name sponsor, name beneficiary);

        // ACTION cancelescrow(name sponsor, name beneficiary);

        ACTION claim(name beneficiary);

        ACTION withdraw(name sponsor, asset quantity);

        [[eosio::on_notify("*::transfer")]]
        void ontransfer(name from, name to, asset quantity, string memo);

    private:
        symbol seeds_symbol = symbol("SEEDS", 4);
        // enum TypeCreate:uint8_t { createNormal = 0, createEscrow = 1 };

        // events scoped by sponsor
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
            asset   locked_balance;
            asset   liquid_balance;

            uint64_t primary_key() const { return sponsor.value; }
        };

        typedef eosio::multi_index<"sponsors"_n, sponsors_table> sponsors_tables;

        token_lock_table locks;
        sponsors_tables sponsors;

        // void createaux(name sponsor, name beneficiary, asset quantity, uint64_t vesting_date, uint8_t type);
        // void cancelaux(name sponsor, name beneficiary);
        void check_asset(asset quantity);
        void init_balance(name user);
};

// extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
//   if (action == name("transfer").value && code == contracts::token.value) {
//       execute_action<vstandescrow>(name(receiver), name(code), &vstandescrow::ontransfer);
//   } else if (code == receiver) {
//       switch (action) {
//           EOSIO_DISPATCH_HELPER(vstandescrow, (reset)(create)(createescrow)(cancel)(cancelescrow)(claim)(withdraw))
//       }
//   }
// }

