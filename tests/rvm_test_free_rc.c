/* 
 * --- TEST ---
 * This tests the rmv_free command
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <backends/ramcloud_backend.h>
#include <rvm.h>
#include <log.h>
#include <error.h>

#define ARR_SIZE 10

rvm_cfg_t* initialize_rvm(char* host, char* port) {
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;

    /* Non-recovery case */
    opt.recovery = false;

    rvm_cfg_t *cfg = rvm_cfg_create(&opt, create_ramcloud_layer);
    CHECK_ERROR(cfg == NULL, 
            ("FAILURE: Failed to initialize rvm configuration - %s\n", strerror(errno)));

    return cfg;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: %s <server-address> <server-port> <restart? (y/n)>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    printf("initialize_rvm\n");
    rvm_cfg_t* cfg = initialize_rvm(argv[1], argv[2]);

    printf("rvm_alloc\n");
    int *safe_arr0 = (int*)rvm_alloc(cfg, ARR_SIZE*sizeof(int));
    CHECK_ERROR(safe_arr0 == NULL, 
            ("FAILURE: Failed to allocate array outside of a txn - %s\n", strerror(errno)));

    memset(safe_arr0, 0, ARR_SIZE*sizeof(int));

    printf("rvm_free\n");
    int ret = rvm_free(cfg, safe_arr0);

    CHECK_ERROR(ret == false,
            ("Failure: error freeing block\n"));


    printf("SUCCESS\n");
    return EXIT_SUCCESS;
}


