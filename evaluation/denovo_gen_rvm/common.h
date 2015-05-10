#ifndef _COMMON_H_
#define _COMMON_H_

#include <errno.h>
#include <assert.h>
#include <utils/error.h>
#include <rvm.h>
#include "contig_generation.h"

extern rvm_cfg_t *cfg;

extern int fa_freq;
extern int cp_freq;

/* How frequently to print progress updates (in #kmers) */
#define PRINT_FREQ 1000000

typedef enum
{
    INIT,   /**< Initialization, before build or probe */
    BUILD,  /**< Build the hash table from file */
    PROBE,  /**< Probe into the hash table to make contigs */
    DONE
} phase_t;

typedef struct
{
    phase_t phase;  /**< Current Phase of Execution */

    /* Genome Data */
    memory_heap_t *memheap; /**< Memory heap for hashtable */
    hash_table_t  *tbl;     /**< Hash table of kmers */
    start_kmer_t *start_list; /**< List of start kmers */

    /* Performance Measurements */
    double inputTime;
    double constrTime;
    double traversalTime;

    /* Pointer to phase-specific state info */
    void *pstate;
} gen_state_t;

/* Start/commit a txn */
#define TX_START(TXID) do {                                      \
        TXID = rvm_txn_begin(cfg);                               \
        CHECK_ERROR(TXID < 0,                                    \
                 ("FAILURE: Could not start transaction - %s\n", \
                 strerror(errno)));                              \
        } while(0)

#define TX_COMMIT(TXID)   do {                                             \
        bool TX_COMMIT_res = rvm_txn_commit(cfg, TXID);                    \
        CHECK_ERROR(!TX_COMMIT_res, ("FAILURE: Could not commit txn\n"))   \
        } while(0)

#endif
