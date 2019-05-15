#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <abieos_numeric.hpp>

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
            requests(receiver, receiver.value)
            {}

        ACTION adduser(name account);
        
        ACTION addapp(name account);
        
        ACTION addrequest(name app, name user, string owner_key, string active_key);
        
        ACTION fulfill(name app, name user);
    private:
        void buyaccount(name account, string owner_key, string active_key);
    
        symbol network_symbol = symbol("EOS", 4);
    
        TABLE app_table {
          name account;
          
          uint64_t primary_key()const { return account.value; }
        };
    
        TABLE user_table {
          name account;

          uint64_t primary_key()const { return account.value; }
        };
        
        TABLE request_table {
          name app;
          name user;
          string owner_key;
          string active_key;
          
          uint64_t primary_key()const { return user.value; }
        };

        typedef eosio::multi_index<"users"_n, user_table> user_tables;
        typedef eosio::multi_index<"apps"_n, app_table> app_tables;
        typedef eosio::multi_index<"requests"_n, request_table> request_tables;

        user_tables users;
        app_tables apps;
        request_tables requests;
};

EOSIO_DISPATCH(accounts, (adduser)(addapp)(addrequest)(fulfill));