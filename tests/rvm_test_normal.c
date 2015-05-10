/* This is a test implementation of the RVM implementation. It doesn't do
 * anything interesting but it uses most of the features */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rvm.h>
#include <rmem_backend.h>
#include <log.h>
#include <error.h>

//#include "malloc_simple.h"
#include "buddy_malloc.h"

#define ARR_SIZE 2048

static void fill_arr(int *a)
{
    int i = 0;
    for(;i < ARR_SIZE; i++)
    {
        a[i]++;
    }

    return;
}

/* Check if the array is filled with the expeceted value. */
static bool check_arr(int *a, int expect, int size)
{
    int i = 0;
    for(;i < ARR_SIZE; i++)
    {
        if(a[i] != expect) {
            if(a[i] == expect - 1)
                printf("FAILURE: Array didn't receive any commits\n");
            else if(a[i] == expect + 1)
                printf("FAILURE: Array has uncommitted changes\n");
            else
                printf("FAILURE: Array has unexpected value: %d\n", a[i]);

            return false;
        }
    }

    return true;
}

rvm_cfg_t* initialize_rvm(char* host, char* port) {
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;
//    opt.alloc_fp = simple_malloc;
//    opt.free_fp = simple_free;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
        
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
    if (argc != 4) {
        printf("usage: %s <server-address> <server-port> <restart? (y/n)>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    bool restart = (strcmp(argv[3],"y") == 0 || strcmp(argv[3],"Y") == 0) ? true : false;

    rvm_opt_t opt;
    opt.host = argv[1];
    opt.port = argv[2];

    if(restart) {
        /* Try to recover from server */
        opt.recovery = true;
        rvm_cfg_t *cfg = rvm_cfg_create(&opt, create_rmem_layer);

        /* Get the new addresses for arr0 and arr1 */
        int **arr_ptr = (int**)rvm_get_usr_data(cfg);
        if(arr_ptr == NULL) {
            printf("FAILURE: pointer to arrays is null!\n");
            return EXIT_FAILURE;
        }

        if(arr_ptr[0] == NULL || arr_ptr[1] == NULL) {
            printf("FAILURE: pointer to arrays corrupted\n");
            return EXIT_FAILURE;
        }

        /* Check their values */
        if(!check_arr(arr_ptr[0], 1, ARR_SIZE)) {
            printf("FAILURE: Arr0 Doesn't look right\n");
            return EXIT_FAILURE;
        }

        /* Check their values */
        if(!check_arr(arr_ptr[1], 2, ARR_SIZE)) {
            printf("FAILURE: Arr1 doesn't look right\n");
            return EXIT_FAILURE;
        }

        printf("SUCCESS: Memory recovered after \"failure\"\n");
    } else {
        rvm_cfg_t* cfg = initialize_rvm(argv[1], argv[2]);

        LOG(8,("rvm_txn_begin\n"));
        rvm_txid_t txid = rvm_txn_begin(cfg);
        CHECK_ERROR(txid < 0,
                ("FAILURE: Could not start transaction - %s\n", strerror(errno)));
       
        /* Allocate a "state" structure to test pointers */
        LOG(8, ("Allocating state structure\n"));
        int **arr_ptr = (int**)rvm_alloc(cfg, 2*sizeof(int*));
        CHECK_ERROR(arr_ptr == NULL,
                ("FAILURE: Failed to allocate tracking structure -%s\n",
                 strerror(errno)));

        /* Register the state structure as our usr_data with rvm */
        rvm_set_usr_data(cfg, arr_ptr);

        /* Arr0 gets incremented once */
        LOG(8,("rvm_alloc\n"));
        arr_ptr[0] = (int*)rvm_alloc(cfg, ARR_SIZE*sizeof(int));
        CHECK_ERROR(arr_ptr[0] == NULL, 
                ("FAILURE: Failed to allocate array0 - %s\n", strerror(errno)));

        /* Arr1 gets incremented twice */
        LOG(8, ("rvm_alloc\n"));
        arr_ptr[1] = (int*)rvm_alloc(cfg, ARR_SIZE*sizeof(int));
        CHECK_ERROR(arr_ptr[1] == NULL,
                ("Failed to allocate array1 - %s", strerror(errno)));

        LOG(8, ("rvm_alloc done\n"));

        //Initialize arrays
        memset(arr_ptr[0], 0, ARR_SIZE*sizeof(int));
        memset(arr_ptr[1], 0, ARR_SIZE*sizeof(int));

        //fill_arr doesn't need to know about rvm
        fill_arr(arr_ptr[0]);
        fill_arr(arr_ptr[1]);
        fill_arr(arr_ptr[1]);

        printf("rvm_txn_commit\n");
        CHECK_ERROR(!rvm_txn_commit(cfg, txid),
                ("FAILURE: Failed to commit transaction - %s", strerror(errno)));
        
        fprintf(stderr, "Check txn commit after commit\n"); 
        CHECK_ERROR(check_txn_commit(cfg, txid) == false,
                ("FAILURE: commit did not get through - %s\n", strerror(errno)));

        printf("rvm_txn_begin\n");
        /* Start a new transaction */
        txid = rvm_txn_begin(cfg);
        fill_arr(arr_ptr[0]);
        fill_arr(arr_ptr[1]);

        //Ohs Noes! Our program has mysteriously crashed without committing!
        printf("SUCCESS: Normal execution proceded without error. "
                "Test now exiting mid-transaction to simulate a failure\n");
        return EXIT_SUCCESS;
    }
    return 0;
}
