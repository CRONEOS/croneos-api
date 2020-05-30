

#pragma once
#include <eosio/eosio.hpp>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/time.hpp>
#include <eosio/permission.hpp>
#include <eosio/singleton.hpp>

/***************************
croneos api config
***************************/
#define _CONTRACT_NAME_ "test" //the class name of your contract
#define _ENABLE_INTERNAL_QUEUE_ 

//NETWORK CONFIG
/*mainnet*/
//#define _CRONEOS_CONTRACT_ "cron.eos"
//#define _DEFAULT_EXEC_ACC_ "croneosexec1"

/*jungle*/
#define _CRONEOS_CONTRACT_ "croncron1111"
#define _DEFAULT_EXEC_ACC_ "execexecexec"

/***************************
end croneos api config
***************************/

namespace croneos{

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

    namespace oracle{
        struct source{
            std::string api_url;
            std::string json_path; //https://www.npmjs.com/package/jsonpath
        };
    }

    //************************
    //struct [[eosio::table, eosio::contract(_MY_CONTRACT_)]] cronjobs{
    struct cronjobs {//is scoped
        uint64_t id;
        eosio::name owner;
        eosio::name tag;
        eosio::name auth_bouncer;
        std::vector<eosio::action> actions;
        eosio::time_point_sec submitted;
        eosio::time_point_sec due_date;
        eosio::time_point_sec expiration;
        eosio::asset gas_fee;
        std::string description;
        uint8_t max_exec_count=1;
        std::vector<croneos::oracle::source> oracle_srcs;

        uint64_t primary_key() const { return id; }
        uint64_t by_owner() const { return owner.value; }
        uint64_t by_due_date() const { return due_date.sec_since_epoch(); }
        uint128_t by_owner_tag() const { return (uint128_t{owner.value} << 64) | tag.value; }
    };
    typedef eosio::multi_index<"cronjobs"_n, cronjobs,
        eosio::indexed_by<"byowner"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_owner>>,
        eosio::indexed_by<"byduedate"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_due_date>>,
        eosio::indexed_by<"byownertag"_n, eosio::const_mem_fun<cronjobs, uint128_t, &cronjobs::by_owner_tag>>
    > cronjobs_table;
    //************************

#ifdef _ENABLE_INTERNAL_QUEUE_

    struct [[eosio::table, eosio::contract(_CONTRACT_NAME_)]] croneosqueue{
        uint64_t id;
        eosio::name tag;
        eosio::action action;
        eosio::time_point_sec due_date;
        eosio::time_point_sec expiration;

        uint64_t primary_key() const { return id; }
        uint64_t by_tag() const { return tag.value; }
        uint64_t by_due_date() const { return due_date.sec_since_epoch(); }
        uint64_t by_expiration() const { return expiration.sec_since_epoch(); }
    };
    
    typedef eosio::multi_index<"croneosqueue"_n, croneosqueue,
        eosio::indexed_by<"bytag"_n, eosio::const_mem_fun<croneosqueue, uint64_t, &croneosqueue::by_tag>>,
        eosio::indexed_by<"byduedate"_n, eosio::const_mem_fun<croneosqueue, uint64_t, &croneosqueue::by_due_date>>,
        eosio::indexed_by<"byexpiration"_n, eosio::const_mem_fun<croneosqueue, uint64_t, &croneosqueue::by_expiration>>
    > croneosqueue_table;
    //************************

    struct [[eosio::table, eosio::contract(_CONTRACT_NAME_)]] croneosstats {
        uint64_t total_count = 0;
        uint64_t exec_count = 0;
        uint64_t cancel_count = 0;
        uint64_t expired_count = 0;
    };
    typedef eosio::singleton<"croneosstats"_n, croneosstats> croneosstats_table;
    //************************
#endif

    eosio::permission_level const default_exec_permission_level = eosio::permission_level{eosio::name(_DEFAULT_EXEC_ACC_), "active"_n};

    void deposit(eosio::name owner, eosio::extended_asset gas, eosio::permission_level auth){
        if(gas.quantity.amount > 0){
            eosio::action(
                auth, 
                gas.contract, 
                "transfer"_n, 
                make_tuple(owner, eosio::name(_CRONEOS_CONTRACT_), gas.quantity, std::string("deposit gas"))
            ).send();
        }
    };



    struct job{
        //job configs configs
        eosio::name owner;//owner and ram payer for storing the cronjob
        eosio::name tag = eosio::name(0);
        eosio::name auth_bouncer = eosio::name(0);
        eosio::name scope = eosio::name(0);
        eosio::time_point_sec due_date = eosio::time_point_sec(0);
        uint32_t delay_sec = 0;
        eosio::time_point_sec expiration = eosio::time_point_sec(0);
        uint32_t expiration_sec = 0;
        eosio::extended_asset gas_fee = eosio::extended_asset( eosio::asset(0, eosio::symbol(eosio::symbol_code("EOS"), 4) ), eosio::name("eosio.token") );
        std::string description ="No description";
        std::vector<eosio::permission_level> custom_exec_permissions;
        uint8_t max_exec_count = 1;
        bool auto_pay_gas = false;

        std::vector<croneos::oracle::source> oracle_sources{};
    

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
                eosio::name(_CRONEOS_CONTRACT_), "schedule"_n,
                make_tuple(
                        owner,
                        scope,
                        tag, 
                        auth_bouncer, 
                        cron_actions, 
                        due_date, 
                        delay_sec, 
                        expiration, 
                        expiration_sec, 
                        gas_fee.quantity, 
                        description, 
                        oracle_sources
                )
            ).send();
            
        }
        
        
        static void cancel_by_tag(eosio::name owner, eosio::name tag, eosio::name scope, eosio::permission_level auth){
            scope = scope == eosio::name(0) ? eosio::name(_CRONEOS_CONTRACT_) : scope;
            cronjobs_table _cronjobs(eosio::name(_CRONEOS_CONTRACT_), scope.value);
            auto by_owner_tag = _cronjobs.get_index<"byownertag"_n>();
            uint128_t composite_id = (uint128_t{owner.value} << 64) | tag.value;
            auto tag_itr = by_owner_tag.find( composite_id);
            if(tag_itr != by_owner_tag.end() ){
                eosio::action(
                    auth,
                    eosio::name(_CRONEOS_CONTRACT_), "cancel"_n,
                    std::make_tuple(owner, tag_itr->id, scope )
                ).send();
            }    
        }
        
        

        private:
        std::vector<eosio::permission_level> construct_permission_levels(){

            if(custom_exec_permissions.size()==0){
                return {default_exec_permission_level};
            }
            else{
                return custom_exec_permissions;
            }
        }

    };


#ifdef _ENABLE_INTERNAL_QUEUE_
    struct queue{
        eosio::name owner;

        queue(eosio::name Owner) { 
            owner = Owner;
        }
        //schedules an action in the queue
        void schedule(eosio::action action, eosio::name tag, eosio::extended_asset gas_fee, uint32_t delay_sec, uint32_t expiration_sec){
            
            eosio::time_point_sec due_date = eosio::time_point_sec(now.sec_since_epoch() + delay_sec);
            eosio::time_point_sec expiration = eosio::time_point_sec(now.sec_since_epoch() + delay_sec + expiration_sec);
            croneos::croneosqueue_table _croneosqueue(owner, owner.value);
            
            croneos::croneosstats_table _croneosstats(owner, owner.value);
            auto s = _croneosstats.get_or_default();
            uint64_t queue_id = s.total_count;
            s.total_count++;
            _croneosstats.set(s, owner);

            clean_expired(_croneosqueue, 50);

            _croneosqueue.emplace(owner, [&](auto &n) {
                n.id = queue_id;
                n.tag = tag;
                n.action = action;
                n.due_date = due_date;
                n.expiration = expiration;
            });
            std::vector<eosio::action> cron_actions;
            eosio::action cron_action = eosio::action(eosio::permission_level{eosio::name(_DEFAULT_EXEC_ACC_), "active"_n}, owner, eosio::name("tick"), std::make_tuple() );
            cron_actions.push_back(cron_action);

            eosio::action(
                eosio::permission_level{owner, "active"_n},
                eosio::name(_CRONEOS_CONTRACT_), "schedule"_n,
                std::make_tuple(
                        owner, //owner
                        eosio::name(_CRONEOS_CONTRACT_), //scope
                        eosio::name(queue_id), //tag
                        eosio::name(0), //auth_bouncer 
                        cron_actions, //cron_actions
                        eosio::time_point_sec(0), //due_date
                        delay_sec, 
                        eosio::time_point_sec(0), //expiration 
                        expiration_sec, 
                        gas_fee.quantity, 
                        std::string("tick"), 
                        std::vector<croneos::oracle::source>{}
                )
            ).send();

        }
        //exec 1 action from the queue
        void exec(){

            croneosqueue_table _croneosqueue(owner, owner.value);
            
            auto by_due_date = _croneosqueue.get_index<"byduedate"_n>();
            if(by_due_date.begin()->expiration < now){
                clean_expired(_croneosqueue, 500);
            }
            auto itr_first = by_due_date.begin();
            if(itr_first != by_due_date.end() ){
                if(itr_first->due_date <= now){

                    if(!eosio::has_auth(eosio::name(_DEFAULT_EXEC_ACC_)) ){
                        //delete from croneos table
                        croneos::job::cancel_by_tag(owner, eosio::name(itr_first->id), eosio::name(_CRONEOS_CONTRACT_), eosio::permission_level{owner, "active"_n});
                    }
                    itr_first->action.send();
                    by_due_date.erase(itr_first);

                    croneos::croneosstats_table _croneosstats(owner, owner.value);
                    auto s = _croneosstats.get();
                    s.exec_count++;
                    _croneosstats.set(s, owner);
                }
                else{
                    eosio::check(false, "queued action not ready yet, wait "+ std::to_string(itr_first->due_date.sec_since_epoch() - now.sec_since_epoch() )+"sec");
                }
            }
            else{
               eosio::check(false, "action queue is empty");
            }
        }

        void cancel(uint64_t queue_id){
            croneosqueue_table _croneosqueue(owner, owner.value);
            auto itr = _croneosqueue.find(queue_id);
            if(itr != _croneosqueue.end() ){
                croneos::job::cancel_by_tag(owner, eosio::name(itr->id), eosio::name(_CRONEOS_CONTRACT_), eosio::permission_level{owner, "active"_n});
                _croneosqueue.erase(itr);

                croneos::croneosstats_table _croneosstats(owner, owner.value);
                auto s = _croneosstats.get();
                s.cancel_count++;
                _croneosstats.set(s, owner);
            }
        }
        
        bool clean_expired(int32_t batch_size){
            croneosqueue_table _croneosqueue(owner, owner.value);
            return clean_expired(_croneosqueue, batch_size);
        }
        

        private:
        eosio::time_point_sec now = eosio::time_point_sec(eosio::current_time_point());
        bool clean_expired(croneosqueue_table& idx, uint32_t batch_size){
            auto by_expiration = idx.get_index<"byexpiration"_n>();
            auto stop_itr = by_expiration.upper_bound(now.sec_since_epoch()+1 );
            uint32_t counter = 0;
            croneos::croneosstats_table _croneosstats(owner, owner.value);
            auto s = _croneosstats.get();
            auto itr = by_expiration.begin();
            while (itr != stop_itr && counter++ < batch_size){
                //croneos::job::cancel_by_tag(owner, eosio::name(itr->id), eosio::name(_CRONEOS_CONTRACT_), eosio::permission_level{owner, "active"_n});
                itr = by_expiration.erase(itr);
                s.expired_count++;
            }
            _croneosstats.set(s, owner);

            return counter > 0 ? true : false;
        }

    };
#endif

}