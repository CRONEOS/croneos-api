

#pragma once
#include <eosio/eosio.hpp>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/time.hpp>
#include <eosio/permission.hpp>

namespace croneos{
    //using namespace std;
    //using namespace eosio;

    //This name space can be savely removed (if not using it's members) to save RAM on the host contract/account
    namespace utils{
        eosio::checksum256 get_trx_id(){
            auto size = eosio::transaction_size();
            char* buffer = (char*)( 512 < size ? malloc(size) : alloca(size) );
            uint32_t read = eosio::read_transaction( buffer, size );
            eosio::check( size == read, "ERR::READ_TRANSACTION_FAILED::read_transaction failed");
            eosio::checksum256 trx_id = eosio::sha256(buffer, read);
            return trx_id;
        }
    }

    eosio::name const cron_contract_name = eosio::name("piecestest12");
    eosio::permission_level const required_exec_permission_level = eosio::permission_level{"execexecexec"_n, "active"_n};

    void deposit(eosio::name owner, eosio::asset gas, eosio::permission_level auth){

        if(gas.amount > 0){
            eosio::action(auth, "eosio.token"_n, "transfer"_n, make_tuple(owner, cron_contract_name, gas, std::string("deposit gas"))).send();
        }
    };

    struct job{
        //job configs configs
        eosio::name owner;//owner and ram payer for storing the cronjob
        eosio::name tag = eosio::name(0);
        eosio::time_point_sec due_date = eosio::time_point_sec(0);
        uint32_t delay_sec = 0;
        eosio::time_point_sec expiration = eosio::time_point_sec(0);
        uint32_t expiration_sec = 0;
        eosio::asset gas_fee = eosio::asset(0, eosio::symbol(eosio::symbol_code("EOS"), 4));//optional: the gas fee you are willing to pay
        std::string description ="This is the default description";//optional:describe the cronjob, visible in UI
        std::vector<eosio::permission_level> custom_exec_permissions;
        bool auto_pay_gas = false;

        template<typename... T>
        void schedule(eosio::name code, eosio::name actionname, std::tuple<T...> data, eosio::permission_level auth) {
            
            std::vector<eosio::action> cron_actions;
            eosio::action cron_action = eosio::action(construct_permission_levels(), code, actionname, move(data) );
            cron_actions.push_back(cron_action);

            //pay gas
            if(auto_pay_gas){
                deposit(owner, gas_fee, auth);
            }
            //schedule
            eosio::action(
                auth,
                cron_contract_name, "schedule"_n,
                make_tuple(owner, tag, cron_actions, due_date, delay_sec, expiration, expiration_sec, gas_fee, description)
            ).send();
            
        }
        private:
        std::vector<eosio::permission_level> construct_permission_levels(){
            bool found = false;
            auto it = custom_exec_permissions.begin();
            while (!found && it++ != custom_exec_permissions.end())
            found = *(it - 1) == required_exec_permission_level;
            if (!found){
                custom_exec_permissions.push_back(required_exec_permission_level);
            }
            return custom_exec_permissions;
        }

    };
}

