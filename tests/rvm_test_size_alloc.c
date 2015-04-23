/* 
 * ----- TEST ------
 * Test rvm_alloc with different sizes (not only page size)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rvm.h>
#include <log.h>
#include <error.h>

#define ARR_SIZE 10

static void fill_arr(int *a, int size)
{
    int i = 0;
    for(;i < size; i++)
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

    rvm_txid_t txid = rvm_txn_begin(cfg);
    CHECK_ERROR(txid < 0,
            ("FAILURE: Could not start transaction - %s\n", strerror(errno)));

    int *safe_arr1 = rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    int *safe_arr2 = rvm_alloc(cfg, 0);
    int *safe_arr3 = rvm_alloc(cfg, 1*sizeof(int));
    int *safe_arr4 = rvm_alloc(cfg, 2*ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr1 == NULL,
            ("Failed to allocate array inside a txn - %s", strerror(errno)));
    CHECK_ERROR(safe_arr2 != NULL,
            ("Allocated array with size 0 - %s", strerror(errno)));
    CHECK_ERROR(safe_arr3 == NULL,
            ("Failed to allocate array inside a txn - %s", strerror(errno)));

    memset(safe_arr1, 0, ARR_SIZE*sizeof(int));
    memset(safe_arr3, 0, 1*sizeof(int));
    memset(safe_arr4, 0, 2*ARR_SIZE*sizeof(int));

    //fill_arr doesn't need to know about rvm
    fill_arr(safe_arr1, ARR_SIZE);
    fill_arr(safe_arr3, 1);
    fill_arr(safe_arr1, 2*ARR_SIZE);
    
    CHECK_ERROR(check_txn_commit(cfg, txid) == true,
            ("FAILURE: data got through before commit - %s", strerror(errno)));

    CHECK_ERROR(!rvm_txn_commit(cfg, txid),
            ("FAILURE: Failed to commit transaction - %s", strerror(errno)));

    CHECK_ERROR(check_txn_commit(cfg, txid) == false,
            ("FAILURE: commit did not get through - %s", strerror(errno)));

    printf("SUCCESS\n");
    return EXIT_SUCCESS;
}

