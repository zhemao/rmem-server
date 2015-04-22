/* This is a test implementation of the RVM implementation. It doesn't do
 * anything interesting but it uses all the features */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rvm.h>
#include <log.h>
#include <error.h>

#define ARR_SIZE 10

static void fill_arr(int *a)
{
    int i = 0;
    for(;i < ARR_SIZE; i++)
    {
        a[i]++;
    }

    return;
}

rvm_cfg_t* initialize_rvm(char* host, char* port) 
{
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;

    /* Non-recovery case */
    opt.recovery = false;

    LOG(8, ("rvm_cfg_create\n"));
    rvm_cfg_t *cfg = rvm_cfg_create(&opt);
    CHECK_ERROR(cfg == NULL, 
            ("FAILURE: Failed to initialize rvm configuration - %s\n", strerror(errno)));

    return cfg;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: %s <server-address> <server-port>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    rvm_cfg_t* cfg = initialize_rvm(argv[1], argv[2]);

    LOG(8,("rvm_alloc\n"));
    //You can allocate outside a txn so long as you don't modify
    int *safe_arr0 = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr0 == NULL, 
            ("FAILURE: Failed to allocate array outside of a txn - %s\n", strerror(errno)));

    LOG(8,("rvm_txn_begin\n"));
    rvm_txid_t txid = rvm_txn_begin(cfg);
    CHECK_ERROR(txid < 0,
            ("FAILURE: Could not start transaction - %s\n", strerror(errno)));

    LOG(8, ("rvm_alloc\n"));
    /* You can also allocate within a transaction */
    int *safe_arr1 = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr1 == NULL,
            ("Failed to allocate array inside a txn - %s", strerror(errno)));

    LOG(8, ("rvm_alloc done\n"));

    //Initialize arrays
    memset(safe_arr0, 0, ARR_SIZE*sizeof(int));
    memset(safe_arr1, 0, ARR_SIZE*sizeof(int));

    //fill_arr doesn't need to know about rvm
    fill_arr(safe_arr0);
    fill_arr(safe_arr1);

    printf("rvm_txn_commit\n");
    CHECK_ERROR(!rvm_txn_commit(cfg, txid),
            ("FAILURE: Failed to commit transaction - %s", strerror(errno)));

    CHECK_ERROR(check_txn_commit(cfg, txid) == false,
            ("FAILURE: commit did not get through - %s", strerror(errno)));

    printf("rvm_txn_begin\n");
    /* Start a new transaction */
    txid = rvm_txn_begin(cfg);
    fill_arr(safe_arr0);
    fill_arr(safe_arr1);

    //Ohs Noes! Our program has mysteriously crashed without committing!
    printf("SUCCESS: Normal execution proceded without error. "
            "Test now exiting mid-transaction to simulate a failure\n");
    return EXIT_SUCCESS;
}

