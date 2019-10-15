#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <abieos_numeric.hpp>
#include "seeds.harvest.types.hpp"
#include "seeds.accounts.types.hpp"

using namespace eosio;
using std::string;

using abieos::keystring_authority;
using abieos::authority;

CONTRACT accounts : public contract {
public:
    using contract::contract;
    accounts(name receiver, name code, datastream<const char*> ds)
    : contract(receiver, code, ds),
    apps(receiver, receiver.value),
    users(receiver, receiver.value),
    requests(receiver, receiver.value),
    refs(receiver, receiver.value),
    reps(receiver, receiver.value),
    balances(name("seedshrvst11"), name("seedshrvst11").value)
    {}
    
    ACTION reset();
    
    ACTION joinuser(name account);
    
    ACTION adduser(name account, string nickname);
    
    ACTION addapp(name account);
    
    ACTION addrequest(name app, name user, string owner_key, string active_key);
    
    ACTION fulfill(name app, name user);
    
    ACTION makeresident(name user);
    
    ACTION makecitizen(name user);
    
    ACTION update(name user, name type, string nickname, string image, string story, string roles, string skills, string interests);
    
    ACTION addref(name referrer, name invited);
    
    ACTION addrep(name user, uint64_t amount);
    
    ACTION migrate(name account,
                   name status,
                   name type,
                   string nickname,
                   string image,
                   string story,
                   string roles,
                   string skills,
                   string interests,
                   uint64_t reputation);
    
private:
    
    IMPORT_ACCOUNTS_TYPES

    symbol seeds_symbol = symbol("SEEDS", 4);
    
    void buyaccount(name account, string owner_key, string active_key);
    
    symbol network_symbol = symbol("TLOS", 4);
    
    TABLE ref_table {
        name referrer;
        name invited;
        
        uint64_t primary_key() const { return referrer.value; }
    };
    
    TABLE request_table {
        name app;
        name user;
        string owner_key;
        string active_key;
        
        uint64_t primary_key()const { return user.value; }
    };
    
    typedef eosio::multi_index<"requests"_n, request_table> request_tables;
    typedef eosio::multi_index<"refs"_n, ref_table> ref_tables;
    
    struct [[eosio::table]] transaction_stats {
        name account;
        asset transactions_volume;
        uint64_t transactions_number;
        
        uint64_t primary_key()const { return transactions_volume.symbol.code().raw(); }
        uint64_t by_transaction_volume()const { return transactions_volume.amount; }
    };
    
    typedef eosio::multi_index< "trxstat"_n, transaction_stats,
    indexed_by<"bytrxvolume"_n,
    const_mem_fun<transaction_stats, uint64_t, &transaction_stats::by_transaction_volume>>
    > transaction_tables;
    
    // Tables
    user_tables users;
    rep_tables reps;
    app_tables apps;
    ref_tables refs;
    request_tables requests;
    
    // Imported tables
    IMPORT_BALANCE_TABLE_ALL
    balance_tables balances;
    
};

EOSIO_DISPATCH(accounts, (reset)(adduser)(joinuser)(addapp)(addrequest)(fulfill)(makeresident)(makecitizen)(update)(migrate)(addref)(addrep));
