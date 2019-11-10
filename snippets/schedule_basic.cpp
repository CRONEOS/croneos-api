

ACTION yourcontract::schedule(uint32_t delay_sec, uint32_t expiration_sec, name tag) {

    //do your stuff here 
    //...
    //...

    //croneos utils
    checksum256 trx_id = croneos::utils::get_trx_id(); //get trx_id of the current transaction. useful if you want to keep records of the cronjob executions.

    //configure and schedule cronjob
    croneos::job mycronjob;
    mycronjob.owner = get_self();

    //optional tag name(), owner can have only 1 (active) job with the same name. if a job with the same tag is expired it will be replaced (gas_fee will be refunded)
    mycronjob.tag = tag;

    //set execution and expiration with delays
    mycronjob.delay_sec = delay_sec; // uint32_t
    mycronjob.expiration_sec = expiration_sec; // uint32_t will

    //OR use fixed time points (don't set delay_sec and/or expiration_sec when using)
    //mycronjob.due_date = time_point_sec(current_time_point().sec_since_epoch() + 60*60*24*3); //executable after 3 days;
    //mycronjob.expiration = time_point_sec(current_time_point().sec_since_epoch() + (60*60*24*3) + 20); // will expire after 20 sec

    //the gass fee you are willing to pay (refund when expired or cancelled)
    mycronjob.gas_fee = asset(1000, symbol(symbol_code("EOS"), 4));
    //optional pay gas as you go. This will transfer the gas fee as an inline action.
    mycronjob.auto_pay_gas = true; //deposit gas fee for current job. If you have deposits you don't need this.

    /*ADVANCED/OPTIONAL*/
    // -> add extra permission(s) for executing the job
    //mycronjob.custom_exec_permissions.push_back(permission_level{"piecesnbitss"_n, "active"_n});

    //set an accompanying description (displayed in UI)
    mycronjob.description ="This is a recursive cron job.";

    //submit send the job
    mycronjob.schedule(
      name("yourcontract"), //contract that holds the to be scheduled action
      name("schedule"), //its action name
      make_tuple(delay_sec, expiration_sec,  tag ), //the action data
      permission_level{get_self(), "active"_n} //authorization for scheduling NOT for execution of the scheduled job
    );

}