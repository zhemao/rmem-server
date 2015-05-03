/* 
 * ----- TEST ------
 * Test case where the replicated data is big (1000 blocks)
 * Stress test the data structures of the system
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rmem.h>
#include <rvm.h>
#include <log.h>
#include <error.h>

#define ARR_SIZE 10
#define SAFE_ARR_SIZE 1000

int* safe_arr[SAFE_ARR_SIZE];

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

    rvm_txid_t txid = rvm_txn_begin(cfg);
    CHECK_ERROR(txid < 0,
            ("FAILURE: Could not start transaction - %s\n", strerror(errno)));

    int i = 0;
    for (; i < SAFE_ARR_SIZE; ++i) {
        safe_arr[i] = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
        CHECK_ERROR(safe_arr[i] == NULL,
                ("Failed to allocate array inside a txn - %s", strerror(errno)));
        memset(safe_arr[i], 0, ARR_SIZE*sizeof(int));
        fill_arr(safe_arr[i]);
    }

    
    CHECK_ERROR(check_txn_commit(cfg, txid) == true,
            ("FAILURE: data got through before commit - %s", strerror(errno)));

    printf("rvm_txn_commit\n");
    CHECK_ERROR(!rvm_txn_commit(cfg, txid),
            ("FAILURE: Failed to commit transaction - %s", strerror(errno)));

    CHECK_ERROR(check_txn_commit(cfg, txid) == false,
            ("FAILURE: commit did not get through - %s", strerror(errno)));

    printf("SUCCESS\n");
    return EXIT_SUCCESS;
}

