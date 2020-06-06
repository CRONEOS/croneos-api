

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
    namespace oracle{
        struct source{
            std::string api_url;
            std::string json_path; //https://www.npmjs.com/package/jsonpath
        };
    }

    struct cronjobs {
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


    struct queue{
        eosio::name owner;
        croneosstats_table _croneosstats;
        croneosstats stats;

        queue(eosio::name Owner): _croneosstats(Owner, Owner.value)  { 
            owner = Owner;
            stats = _croneosstats.get_or_create(owner, croneosstats() );
        }

        ~queue(){
            _croneosstats.set(stats, owner);
        }

        //schedules an action in the queue
        void schedule(eosio::action action, eosio::name tag, eosio::asset gas_fee, uint32_t delay_sec, uint32_t expiration_sec){
            schedule( action, tag, gas_fee, delay_sec, expiration_sec, 1);
        }

        void schedule(eosio::action action, eosio::name tag, eosio::asset gas_fee, uint32_t delay_sec, uint32_t expiration_sec, uint8_t repeat){
            
            eosio::time_point_sec due_date = eosio::time_point_sec(now.sec_since_epoch() + delay_sec);
            eosio::time_point_sec expiration = eosio::time_point_sec(now.sec_since_epoch() + delay_sec + expiration_sec);
            croneos::croneosqueue_table _croneosqueue(owner, owner.value);
            

            uint64_t start_id = stats.total_count;

            clean_expired(_croneosqueue, 50);

            for(int i = 0; i < repeat; ++i){
                _croneosqueue.emplace(owner, [&](auto &n) {
                    n.id = stats.total_count;
                    n.tag = tag;
                    n.action = action;
                    n.due_date = due_date;
                    n.expiration = expiration;
                });
                stats.total_count++;           
            }

            eosio::action(
                eosio::permission_level{owner, "active"_n},
                croneos_contract, "qschedule"_n,
                std::make_tuple(
                        owner, //owner
                        start_id, //queue_id
                        eosio::name("tick"), //tick_action_name
                        delay_sec, //delay_sec
                        expiration_sec, //expiration_sec
                        gas_fee, //gas_fee asset
                        std::string("multi tick"),
                        repeat
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
                        cancel_by_tag(owner, eosio::name(itr_first->id) );
                    }
                    itr_first->action.send();
                    by_due_date.erase(itr_first);

                    stats.exec_count++;

                }
                else{
                    eosio::check(false, "queued action not ready yet, wait "+ std::to_string(itr_first->due_date.sec_since_epoch() - now.sec_since_epoch() )+" sec");
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
                cancel_by_tag(owner, eosio::name(itr->id) );
                _croneosqueue.erase(itr);

                stats.cancel_count++;
            }
        }
        
        bool clean_expired(int32_t batch_size){
            croneosqueue_table _croneosqueue(owner, owner.value);
            return clean_expired(_croneosqueue, batch_size);
        }
        
        private:
        eosio::time_point_sec now = eosio::time_point_sec(eosio::current_time_point() );
        eosio::name croneos_contract = eosio::name(_CRONEOS_CONTRACT_);

        bool clean_expired(croneosqueue_table& idx, uint32_t batch_size){
            auto by_expiration = idx.get_index<"byexpiration"_n>();
            auto stop_itr = by_expiration.upper_bound(now.sec_since_epoch()+1 );
            uint32_t counter = 0;

            auto itr = by_expiration.begin();
            while (itr != stop_itr && counter++ < batch_size){
                //croneos::job::cancel_by_tag(owner, eosio::name(itr->id), eosio::name(_CRONEOS_CONTRACT_), eosio::permission_level{owner, "active"_n});
                itr = by_expiration.erase(itr);
                stats.expired_count++;
            }

            return counter > 0 ? true : false;
        }

        void cancel_by_tag(eosio::name owner, eosio::name tag){
            cronjobs_table _cronjobs(croneos_contract, croneos_contract.value);
            auto by_owner_tag = _cronjobs.get_index<"byownertag"_n>();
            uint128_t composite_id = (uint128_t{owner.value} << 64) | tag.value;
            auto tag_itr = by_owner_tag.find(composite_id);
            if(tag_itr != by_owner_tag.end() ){
                eosio::action(
                    eosio::permission_level{owner, "active"_n},
                    croneos_contract, "cancel"_n,
                    std::make_tuple(owner, tag_itr->id, croneos_contract )
                ).send();
            }    
        }
    };
}




