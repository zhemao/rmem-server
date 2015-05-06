/* This is a test implementation of the RVM implementation. It doesn't do
 * anything interesting but it uses all the features */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <backends/rmem_backend.h>
#include <rvm.h>
#include <log.h>
#include <error.h>

#define ARR_SIZE 10

static bool check_arr(int *a)
{
    int i = 0;
    for(;i < ARR_SIZE; i++)
    {
        if(a[i] != 1) {
            if(a[i] == 0)
                printf("FAILURE: Array didn't receive any commits\n");
            else if(a[i] == 2)
                printf("FAILURE: Array has uncommitted changes\n");
            else
                printf("FAILURE: Array has unexpected value: %d\n", a[i]);

            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: %s <server-address> <server-port>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    
    rvm_opt_t opt;
    opt.host = argv[1];
    opt.port = argv[2];

    /* Try to recover from server */
    opt.recovery = true;
    rvm_cfg_t *cfg = rvm_cfg_create(&opt, create_rmem_layer);

    /* Get the new addresses for arr0 and arr1 */
    int *safe_arr0 = (int*)rvm_rec(cfg);
    int *safe_arr1 = (int*)rvm_rec(cfg);

    assert(safe_arr0);
    assert(safe_arr1);

    /* Check their values */
    if(!check_arr(safe_arr0)) {
        printf("FAILURE: Array 0 doesn't look right\n");
        return EXIT_FAILURE;
    }

    if(!check_arr(safe_arr1)) {
        printf("FAILURE: Array 1 doesn't look right\n");
        return EXIT_FAILURE;
    }

    printf("Test recovery successfull\n");

    return 0;
}

