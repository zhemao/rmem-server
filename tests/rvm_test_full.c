/* This is a test implementation of the RVM implementation. It doesn't do
 * anything interesting. It's designed to use all features and stress the
 * system somewhat. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rvm.h>
#include <log.h>
#include <error.h>
#include <sys/queue.h>

//#include "malloc_simple.h"
#include "buddy_malloc.h"

/* The sizes of the arrays and linked list used for testing */
/* Biger than a page worth of ints, will use block allocator */
#define ARR0_SIZE 2048
/* Less than a page worth of ints, will use buddy allocator */
#define ARR1_SIZE 128
/* Pretty arbitrary choice of size, but it should stress the buddy allocator */
#define LIST_SZ 512

/* Magic number used to check for memory corruption */
#define MAGIC 0xDEADBEEF

/* Start a txn */
#define TX_START do {                                            \
        txid = rvm_txn_begin(cfg);                               \
        CHECK_ERROR(txid < 0,                                    \
                 ("FAILURE: Could not start transaction - %s\n", \
                 strerror(errno)));                              \
        } while(0)

#define TX_COMMIT   do {                                                 \
        bool TX_COMMIT_res = rvm_txn_commit(cfg, txid);                  \
        CHECK_ERROR(!TX_COMMIT_res, ("FAILURE: Could not commit txn"))   \
        } while(0)

typedef struct qelem
{
    void *self; /* Always points to itself, used to check mem corruption */
    LIST_ENTRY(qelem) qent;
} qelem_t;

typedef LIST_HEAD(circq, qelem) qhead_t;

typedef struct test_state
{
    /* Which phase are we in? */
    enum phase_t
    {
        PHASE0,
        PHASE1,
        PHASE2,
        PHASE3,
        PHASE4,
        PHASE5,
        PHASE6,
        DONE
    } phase;
    int iter; /* Some phases can iterate */

    qhead_t list;
    int *arr0;
    int *arr1;
} test_state_t;

/* Program Phases */
bool phase0(rvm_cfg_t *cfg, test_state_t *state);
bool phase1(rvm_cfg_t *cfg, test_state_t *state);
bool phase2(rvm_cfg_t *cfg, test_state_t *state);
bool phase3(rvm_cfg_t *cfg, test_state_t *state);
bool phase4(rvm_cfg_t *cfg, test_state_t *state);
bool phase5(rvm_cfg_t *cfg, test_state_t *state);
bool phase6(rvm_cfg_t *cfg, test_state_t *state);

/* Each element in the list gets elem[idx] += idx + v */
static void fill_arr(int *a, int v, size_t size)
{
    int i = 0;
    for(;i < size; i++)
    {
        a[i] += i + v;
    }

    return;
}

/* Check if the array is filled with the expeceted value. */
static bool check_arr(int *a, int expect, int size)
{
    int i = 0;
    for(;i < size; i++)
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

rvm_cfg_t* initialize_rvm(char* host, char* port, bool rec) {
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;
//    opt.alloc_fp = simple_malloc;
//    opt.free_fp = simple_free;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
        
    /* Non-recovery case */
    opt.recovery = rec;

    LOG(8, ("rvm_cfg_create\n"));
    rvm_cfg_t *cfg = rvm_cfg_create(&opt);
    CHECK_ERROR(cfg == NULL, 
            ("FAILURE: Failed to initialize rvm configuration - %s\n", strerror(errno)));

    return cfg;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("usage: %s <server-address> <server-port> <phase[-1 - 6]>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    int start_phase = atoi(argv[3]);

    bool res;

    test_state_t *state = NULL;
    rvm_cfg_t *cfg;
    if(start_phase >= 0) {
        /* Try to recover from server */
        cfg = initialize_rvm(argv[1], argv[2], true);

        /* Recover the state (if any) */
        state = rvm_get_usr_data(cfg);
    } else {
        /* Starting from scratch */
        cfg = initialize_rvm(argv[1], argv[2], false);
        CHECK_ERROR(cfg == NULL, ("Failed to initialize rvm\n"));

        state = NULL;
    }

    rvm_txid_t txid;
    /*====================================================================
     * TX 0 - Allocate and Initialize Arrays
     *===================================================================*/
    /* If state is NULL then we are starting from scratch or recovering from
       an early error */
    if(state == NULL) {
        LOG(8,("Phase 0:\n"));
        TX_START;

        /* Allocate a "state" structure to test pointers */
        state = rvm_alloc(cfg, sizeof(test_state_t));
        CHECK_ERROR(state == NULL, ("FAILURE: Couldn't allocate state\n"));

        /* Initialize the arrays */ 
        res = phase0(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 0 Failure\n"));

        if(start_phase == -1) {
            LOG(8, ("SUCCESS: Phase 0, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        /* End of first txn */
        state->phase = PHASE1;
        TX_COMMIT;
    }

    switch(state->phase) {
    case PHASE1:
        /*====================================================================
         * TX 1 Increment arrays, don't mess with LL
         *===================================================================*/
        LOG(8, ("Phase 1:\n"));
        TX_START;
        
        res = phase1(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 1 failed\n"));

        /* Simulate Failure */
        if(start_phase == 0) {
            LOG(8, ("SUCCESS: Phase 1, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        state->phase = PHASE2;
        TX_COMMIT;
    
    case PHASE2: //Fallthrough
        /*====================================================================
         * TX 2 Free Arrays
         *===================================================================*/
        LOG(8, ("Phase 2:\n"));
        TX_START;

        res = phase2(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 2 failed\n"));

        /* Simulate Failure */
        if(start_phase == 1) {
            LOG(8, ("SUCCESS: Phase 2, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        state->phase = PHASE3;
        TX_COMMIT;

    case PHASE3: //Fallthrough
        /*====================================================================
         * TX 3 Fill in Linked list
         *===================================================================*/
        LOG(8, ("Phase 3:\n"));
        TX_START;

        res = phase3(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 3 failed\n"));

        /* Simulate Failure */
        if(start_phase == 2) {
            LOG(8, ("SUCCESS: Phase 3, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        state->phase = PHASE4;
        TX_COMMIT;

    case PHASE4:
        /*====================================================================
         * TX 4 Free Half the linked list
         *===================================================================*/
        LOG(8, ("Phase 4:\n"));
        TX_START;

        res = phase4(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 4 failed\n"));

         /* Simulate Failure */
        if(start_phase == 3) {
            LOG(8, ("SUCCESS: Phase 4, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        state->phase = PHASE5;
        TX_COMMIT;

    case PHASE5:
        /*====================================================================
         * TX 5 Re-allocate half of the linked list
         *===================================================================*/
        LOG(8, ("Phase 5:\n"));
        TX_START;

        res = phase5(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 5 failed\n"));

         /* Simulate Failure */
        if(start_phase == 4) {
            LOG(8, ("SUCCESS: Phase5, simulating failure\n"));
            return EXIT_SUCCESS;
        }

       state->phase = PHASE6;
        TX_COMMIT;

    case PHASE6:
        /*====================================================================
         * TX 6 Free whole linked list
         *===================================================================*/
        LOG(8, ("Phase 6:\n"));
        TX_START;

        res = phase6(cfg, state);
        CHECK_ERROR(!res, ("FAILURE: Phase 6 failed\n"));

        /* Simulate Failure */
        if(start_phase == 5) {
            LOG(8, ("SUCCESS: Phase 6, simulating failure\n"));
            return EXIT_SUCCESS;
        }

        state->phase = DONE;
        TX_COMMIT;

    case DONE:
        LOG(8, ("SUCCESS: Got through all phases\n"));
        break;

    default:
        LOG(1, ("FAILURE: Corrupted State"));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Phase 0 initializes the state */
bool phase0(rvm_cfg_t *cfg, test_state_t *state)
{
    /* Register the state structure as our usr_data with rvm */
    rvm_set_usr_data(cfg, state);

    /* Initialize an empty list */
    LIST_INIT(&state->list);

    /* Allocating big array */
    LOG(8,("Allocating array 0. It's a bigun\n"));
    state->arr0 = rvm_alloc(cfg, ARR0_SIZE*sizeof(int));
    RETURN_ERROR(state->arr0 == NULL, false,
            ("FAILURE: Failed to allocate array0 - %s\n", strerror(errno)));

    /* Allocating smaller array */
    LOG(8, ("Allocating array 1, smaller arraay\n"));
    state->arr1 = rvm_alloc(cfg, ARR1_SIZE*sizeof(int));
    RETURN_ERROR(state->arr1 == NULL, false,
            ("Failed to allocate array1 - %s", strerror(errno)));

    //Initialize arrays
    memset(state->arr0, 0, ARR0_SIZE*sizeof(int));
    memset(state->arr1, 0, ARR1_SIZE*sizeof(int));

    return true;
}

/* Phase 1 fills in the arrays, may be repeated */
bool phase1(rvm_cfg_t *cfg, test_state_t *state)
{
    //fill_arr doesn't need to know about rvm
    fill_arr(state->arr0, 0, ARR0_SIZE);
    fill_arr(state->arr1, 1, ARR1_SIZE);

    return true;
}

/* Phase 2 Frees the arrs */
bool phase2(rvm_cfg_t *cfg, test_state_t *state)
{
    bool res0, res1;
    
    res0 = rvm_free(cfg, state->arr0);
    res1 = rvm_free(cfg, state->arr1);

    return (res0 && res1);
}

/* Phase 3 Fills in the list */
bool phase3(rvm_cfg_t *cfg, test_state_t *state)
{
    for(int i = 0; i < LIST_SZ; i++)
    {
        qelem_t *elem = rvm_alloc(cfg, sizeof(qelem_t));
        RETURN_ERROR(elem == NULL, false, ("Failed to alloc list elem\n"));

        elem->self = elem;
        LIST_INSERT_HEAD(&(state->list), elem, qent);
    }

    return true;
}

/* Phase 4 Frees every other item in the list */
bool phase4(rvm_cfg_t *cfg, test_state_t *state)
{
    bool res;

    for(qelem_t *e = state->list.lh_first; e != NULL; e = e->qent.le_next)
    {
        qelem_t *tmp = e->qent.le_next;
        
        /* Remove this entry */
        LIST_REMOVE(e, qent);
        res = rvm_free(cfg, e);
        RETURN_ERROR(!res, false, ("Failed to free list elem\n"));

        /* This effectively does every other elem */
        e = tmp;
    }

    return true;
}

/* Phase 5 re-allocates everything free'd in phase 4 */
bool phase5(rvm_cfg_t *cfg, test_state_t *state)
{
    for(int i = 0; i < LIST_SZ / 2; i++)
    {
        qelem_t *elem = rvm_alloc(cfg, sizeof(qelem_t));
        RETURN_ERROR(elem == NULL, false, ("Failed to alloc list elem\n"));

        elem->self = elem;
        LIST_INSERT_HEAD(&(state->list), elem, qent);
    }

    return true;
}

/* Phase 6 frees the list */
bool phase6(rvm_cfg_t *cfg, test_state_t *state)
{
    bool res;
    while(state->list.lh_first != NULL)
    {
        qelem_t *elem = state->list.lh_first;
        LIST_REMOVE(state->list.lh_first, qent);
        res = rvm_free(cfg, elem);
        RETURN_ERROR(!res, false, ("Failed to free list\n"));
    }

    return true;
}

