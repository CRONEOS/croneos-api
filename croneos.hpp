

#pragma once
#include <eosio/eosio.hpp>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/time.hpp>
#include <eosio/permission.hpp>

namespace croneos{
    using namespace std;
    using namespace eosio;
     
    struct job{

        name owner;//owner and ram payer for storing the cronjob
        name tag = name(0);
        time_point_sec due_date = time_point_sec(0);
        uint32_t delay_sec = 0;
        time_point_sec expiration = time_point_sec(0);
        uint32_t expiration_sec = 0;
        asset gass_fee = asset(0, symbol(symbol_code("EOS"), 4));//optional: the gass fee you are willing to pay
        string description ="This is the default description";//optional:describe the cronjob, visible in UI

        vector<permission_level> exec_permission_level = {permission_level{"execexecexec"_n, "active"_n} };//permission level used for executing the cronjob

        bool auto_pay_gass = false;
        

        template<typename... T>
        void schedule(name code, name actionname, tuple<T...> data, permission_level auth) {
            
            vector<action> cron_actions;
            action cron_action = action(exec_permission_level, code, actionname, move(data) );
            cron_actions.push_back(cron_action);

            //pay gass
            if(auto_pay_gass){
                pay_gass(gass_fee, auth);
            }
            //schedule
            action(
                auth,
                "piecestest12"_n, "schedule"_n,
                 make_tuple(owner, tag, cron_actions, due_date, delay_sec, expiration, expiration_sec, gass_fee, description)
            ).send();

            
        }

        void pay_gass(asset gass, permission_level auth){

            if(gass.amount > 0){
                action(
                    auth,
                    "eosio.token"_n, "transfer"_n,
                    make_tuple(owner, "piecestest12"_n, gass, string("pay gass"))
                ).send();
            }
        
        }

    };
}

