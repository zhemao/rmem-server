/* 
 * TEST
 * Test the commit functionality.
 * Make sure data is only in final destination after commit
 * Make sure data is correctly stored after commit
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rvm.h>
#include <rmem.h>
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
    rvm_cfg_t *cfg = rvm_cfg_create(&opt, create_rmem_layer);
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

    int *safe_arr0 = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr0 == NULL, 
            ("FAILURE: Failed to allocate array outside of a txn - %s\n", strerror(errno)));

    rvm_txid_t txid = rvm_txn_begin(cfg);
    CHECK_ERROR(txid < 0,
            ("FAILURE: Could not start transaction - %s\n", strerror(errno)));

    int *safe_arr1 = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr1 == NULL,
            ("Failed to allocate array inside a txn - %s", strerror(errno)));

    for (int i = 0; i < ARR_SIZE; ++i) {
        printf("i: %d\n", i);
        safe_arr0[i] = 0;
    }
    memset(safe_arr0, 0, ARR_SIZE*sizeof(int));
    memset(safe_arr1, 0, ARR_SIZE*sizeof(int));

    fill_arr(safe_arr0);
    fill_arr(safe_arr1);
    
    CHECK_ERROR(check_txn_commit(cfg, txid) == true,
            ("FAILURE: data got through before commit - %s", strerror(errno)));

    CHECK_ERROR(!rvm_txn_commit(cfg, txid),
            ("FAILURE: Failed to commit transaction - %s", strerror(errno)));

    CHECK_ERROR(check_txn_commit(cfg, txid) == false,
            ("FAILURE: commit did not get through - %s", strerror(errno)));

    printf("Success!\n");
    return EXIT_SUCCESS;
}

