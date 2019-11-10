

ACTION yourcontract::schedule(uint32_t delay_sec, uint32_t expiration_sec, name tag) {

    //do your stuff here 
    //...
    //...

    //croneos utils
    checksum256 trx_id = croneos::utils::get_trx_id(); //get trx_id of the current transaction. useful if you want to keep records of the cronjob executions.

    //configure and schedule cronjob
    croneos::job mycronjob;
    mycronjob.owner = get_self();
    mycronjob.tag = tag;//optional tag name(), owner can have only 1 (active) job with the same name. if a job with the same tag is expired it will be replaced (gas_fee will be refunded)
    //set execution and expiration with delays
    mycronjob.delay_sec = delay_sec;
    mycronjob.expiration_sec = expiration_sec; //will expire 20 sec after due_date
    //OR use fixed time points
    //mycronjob.due_date = time_point_sec(current_time_point().sec_since_epoch() + 60*60*24*3); //executable after 3 days;
    //mycronjob.expiration = time_point_sec(current_time_point().sec_since_epoch() + (60*60*24*3) + 20); // will expire after 20 sec

    mycronjob.gas_fee = asset(1000, symbol(symbol_code("EOS"), 4));//the gass fee you are willing to pay (refund when expired or cancelled)
    mycronjob.auto_pay_gas = true;//optional, this will transfer the set amount of gass just before the scheduling. you also can top up your balance beforehand

    /*ADVANCED/OPTIONAL*/
    // -> add extra permission(s) for executing the job
    //mycronjob.custom_exec_permissions.push_back(permission_level{"piecesnbitss"_n, "active"_n});
    mycronjob.description ="This is a recursive cron job.";

    //submit send the job
    mycronjob.schedule(
      name("testhitesthi"), //contract that holds the to be scheduled action
      name("proxschedule"), //its action name
      make_tuple(delay_sec, expiration_sec,  tag ), //the action data
      permission_level{get_self(), "active"_n} //authorization for scheduling NOT execution of the scheduled job, typically this will be the same as owner
    );

}