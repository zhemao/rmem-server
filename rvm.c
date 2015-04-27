/* Implementation of user-facing functions for rvm */
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
#include "rvm.h"
#include "rvm_int.h"
#include "rmem.h"
#include "common.h"
#include "log.h"
#include "error.h"

static inline int rvm_protect(void *addr, size_t size)
{
    return mprotect(addr, size, PROT_READ);
}

static inline int rvm_unprotect(void *addr, size_t size)
{
    return mprotect(addr, size, PROT_READ | PROT_WRITE | PROT_EXEC);
}

/* Global bit-mask of every changed block */
bitmap_t *blk_chlist;

/* Flag to indicate whether we are currently handling a fault */
volatile bool in_sighdl;

rvm_cfg_t *cfg_glob;

/* Signal handler for when blocks are written by the user (handles SIGSEGV) */
void block_write_sighdl(int signum, siginfo_t *siginfo, void *uctx);

/* Recover the block table and all pages
 * TODO Right now this just loads everything into new locations. Eventually this
 * will need to re-write pointers or map stuff to the original address.*/
static bool recover_blocks(rvm_cfg_t *cfg)
{
    int err;

    /* Recover the block table */
    err = rmem_get(&(cfg->rmem), cfg->blk_tbl, cfg->blk_tbl_mr,
            BLOCK_TBL_ID, cfg->blk_sz);
    if(err != 0) {
        rvm_log("Failed to recover block table\n");
        errno = EUNKNOWN;
        return false;
    }

    /* Recover every previously allocated block */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx += 2)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue; //Freed memory

        /* Allocate local storage for recovered block */
        void *new_addr = mmap(blk->local_addr, cfg->blk_sz,
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                -1, 0);
        if(new_addr != blk->local_addr) {
            int er_tmp = errno;
            LOG(1, ("Failed to allocate space for recovered block: %s\n",
                    strerror(errno)));
            errno = er_tmp;
            return false;
        }

        /* Register new memory with IB */
        blk->mr = rmem_create_mr(blk->local_addr, blk->size);
        if(blk->mr == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        /* Actual fetch from server */
        err = rmem_get(&(cfg->rmem), blk->local_addr, blk->mr,
                bx + 1, blk->size);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = EUNKNOWN;
            return false;
        }

        /* Protect the block to detect changes */
        rvm_protect(blk->local_addr, blk->size);
    }

    return true;
}

rvm_cfg_t *rvm_cfg_create(rvm_opt_t *opts)
{
    int err;

    rvm_cfg_t *cfg = malloc(sizeof(rvm_cfg_t));
    if(cfg == NULL)
        return NULL;
    cfg->blk_sz = sysconf(_SC_PAGESIZE);
    cfg->in_txn = false;

    rmem_connect(&(cfg->rmem), opts->host, opts->port);

    /* Allocate and initialize the block table locally */
    err = posix_memalign((void**)&(cfg->blk_tbl), cfg->blk_sz, cfg->blk_sz);
    if(err != 0) {
        errno = err;
        return NULL;
    }

    /* Allocate and initialize the block change list */
    blk_chlist = malloc(BITNSLOTS(BLOCK_TBL_SIZE)*sizeof(int32_t));
    memset(blk_chlist, 0, BITNSLOTS(BLOCK_TBL_SIZE)*sizeof(int32_t));

    /* Register the local block table with IB */
    cfg->blk_tbl_mr = rmem_create_mr(cfg->blk_tbl, cfg->blk_sz);
    if(cfg->blk_tbl_mr == NULL) {
        rvm_log("Failed to register memory for block table\n");
        errno = EUNKNOWN;
        return NULL;
    }

    if(opts->recovery) {
        if(!recover_blocks(cfg))
            return NULL;
    } else {
        /* Allocate and register the block table remotely */
        cfg->blk_tbl->raddr = rmem_malloc(&(cfg->rmem), cfg->blk_sz,
                BLOCK_TBL_ID);

        LOG(5, ("Allocated remote table in raddr: %ld\n", cfg->blk_tbl->raddr));

        if(cfg->blk_tbl->raddr == 0) {
            rvm_log("Failed to allocate remote memory for block table\n");
            errno = EUNKNOWN;
            return NULL;
        }

        cfg->blk_tbl->n_blocks = 0;
    }

    /* Install our special signal handler to track changed blocks */
    in_sighdl = false;
    struct sigaction sigact;
    sigact.sa_sigaction = block_write_sighdl;
    sigact.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&(sigact.sa_mask));
    sigaction(SIGSEGV, &sigact, NULL);

    /* A global config is used because signal handlers need access to it. */
    /* TODO: We probably don't need the cfg argument anymore since there can
     * only be one now. */
    cfg_glob = cfg;
    return cfg;
}

bool rvm_cfg_destroy(rvm_cfg_t *cfg)
{
    if(cfg == NULL) {
        rvm_log("Received Null configuration\n");
        errno = EINVAL;
        return false;
    }

    /* Free all remote blocks (leave local blocks)*/
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx+=2) 
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue;

        rmem_free(&cfg->rmem, bx + 1); // free 'real' block
        rmem_free(&cfg->rmem, bx + 2 ); // free 'shadow' block
        if(blk->mr)
            ibv_dereg_mr(blk->mr);

        /* Unprotect block */
        rvm_unprotect(blk->local_addr, blk->size);
    }

    /* Clean up config info on server */
    rmem_free(&cfg->rmem, BLOCK_TBL_ID);
    ibv_dereg_mr(cfg->blk_tbl_mr);
    rmem_disconnect(&(cfg->rmem));

    /* Free local memory */
    free(blk_chlist);
    free(cfg->blk_tbl);
    free(cfg);

    return true;
}

rvm_txid_t rvm_txn_begin(rvm_cfg_t* cfg)
{
    cfg->in_txn = true;

    //Always return the same txid because we don't support nesting or rollback
    return 1;
}

bool check_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid)
{
    LOG(8, ("Checking txn commit\n"));

    // check block table
    char* block = (char*)malloc(sizeof(char) * cfg->blk_sz);
    RETURN_ERROR(block == 0, false,
            ("Failure: Error allocating table\n"));

    struct ibv_mr* block_mr = rmem_create_mr(block, sizeof(char) * cfg->blk_sz);

    LOG(8, ("Getting block table\n"));
    int err;
    err = rmem_get(&cfg->rmem, block,
                block_mr, BLOCK_TBL_ID, cfg->blk_sz);
    LOG(8, ("Block table fetched\n"));

    RETURN_ERROR(err != 0, false,
            ("Failure: Error doing RDMA read of block table\n"));

    err = memcmp(block, cfg->blk_tbl, sizeof(char) * cfg->blk_sz);

    RETURN_ERROR(err != 0, false,
            ("Block table does not match replica\n"));

    //check data blocks
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx+=2) {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL) // ?
            continue;

        /* TODO Eventually we may have to unpin blocks, this will catch that.
         * Right now this should never trigger. */
        assert(blk->mr != NULL);

        char* block = (char*)malloc(sizeof(char) * blk->size);
        struct ibv_mr* block_mr = rmem_create_mr(block, sizeof(char) * blk->size);

        int err = rmem_get(&(cfg->rmem), block, block_mr,
                bx + 1, blk->size);
        RETURN_ERROR(err != 0, false,
            ("Failure: Error doing RDMA read of block\n"));
        

        err = memcmp(block, blk->local_addr, sizeof(char) * blk->size);
        RETURN_ERROR(err != 0, false,
                ("Block %d does not match replica\n", bx));

        free(block);

    }

    free(block);
    ibv_dereg_mr(block_mr);

    cfg->in_txn = false;

    return true;
}

bool rvm_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid)
{
    /* TODO This is a brain-dead initial implementation. We simply copy over
     * everything in the block table, even if it wasn't modified. The long-term
     * solution is to use protection bits on the page table to automatically
     * detect changes and only copy those.
     */
    int err;

    /* Copy the updated block table */
    err = rmem_put(&(cfg->rmem), BLOCK_TBL_ID, cfg->blk_tbl,
            cfg->blk_tbl_mr, cfg->blk_sz);
    if(err != 0) {
        rvm_log("Failed to write block table\n");
        errno = err;
        return false;
    }

    /* Walk the block table and commit everything */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++)
    {
        /* If the block has been freed or if it hasn't changed, skip it */
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL || !BITTEST(blk_chlist, bx))
            continue;

        /* TODO Eventually we may have to unpin blocks, this will catch that.
         * Right now this should never trigger. */
        assert(blk->mr != NULL);

        err = rmem_put(&(cfg->rmem), bx + 2, blk->local_addr, blk->mr,
                blk->size);
        if(err != 0) {
            rvm_log("Failed to write block %d\n", bx);
            errno = err;
            return false;
        }

        int ret = rmem_multi_cp_add(&cfg->rmem, bx + 1, bx + 2, blk->size);
        CHECK_ERROR(ret != 0,
                ("Failure: rmem_multi_cp_add\n"));

        /* Re-protect the block for the next txn */
        rvm_protect(blk->local_addr, blk->size);
    }

    int ret = rmem_multi_cp_go(&cfg->rmem);
        CHECK_ERROR(ret != 0,
                ("Failure: rmem_multi_cp_go\n"));

    return true;
}

void *rvm_alloc(rvm_cfg_t* cfg, size_t size)
{
    int err;

    /* TODO This is a brain-dead initial implementation, we allocate an entire
     * page for each allocation. Obviously this is wasteful, plus it caps the
     * max allocation at one page. This is just to get a basic working
     * implementation and will be improved later.
     */

    if (size == 0) {
        return NULL;
    }

    if(size > cfg->blk_sz) {
        rvm_log("rvm_alloc currently does not support allocations larger than"
                "the page size\n");
        return NULL;
    }

    /* Allocate and initialize the block locally */
    if(cfg->blk_tbl->n_blocks == BLOCK_TBL_SIZE) {
        //Out of room in the block table
        rvm_log("Block table out of space\n");
        errno = ENOMEM;
        return NULL;
    }

    // search for a free block
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx += 2) 
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            break;
    }
    CHECK_ERROR(bx >= BLOCK_TBL_SIZE,
            ("Failure: Block table is full and we should have caught this earlier\n"));

    block_desc_t *block = &(cfg->blk_tbl->tbl[bx]);

    // set block size
    block->size = size;

    // ----- SHADOW PAGES ------ //
    // final destination of a block has block_id = X (odd numbers, starts at 1)
    // corresponding shadow block has block_id = X + 1
    block->bid = BLOCK_TBL_ID + cfg->blk_tbl->n_blocks + 1;
    cfg->blk_tbl->n_blocks += 2;
    /* Allocate entire blocks at a time because the automatic detection works
       at a page granularity */
    err = posix_memalign(&(block->local_addr), cfg->blk_sz, cfg->blk_sz);
    if(err != 0) {
        errno = err;
        return NULL;
    }

    /* Allocate and register the block table remotely */
    /* TODO Right now we pin everything, all the time. Eventually we may want
     * to have some caching thing where we only pin hot pages or something.
     */
    block->mr = rmem_create_mr(block->local_addr, size);
    if(block->mr == NULL) {
        rvm_log("Failed to register memory for block\n");
        errno = EUNKNOWN;
        return NULL;
    }

    // allocate final block
    block->raddr = rmem_malloc(&(cfg->rmem), size, block->bid);
    // allocate shadow block
    uintptr_t shadow_ptr = rmem_malloc(&(cfg->rmem), size, block->bid + 1);
    if(block->raddr == 0 || shadow_ptr == 0) {
        rvm_log("Failed to allocate remote memory for block\n");
        errno = EUNKNOWN;
        return NULL;
    }

    /* Protect the local block so that we can keep track of changes */
    rvm_protect(block->local_addr, block->size);

    return block->local_addr;
}

bool rvm_free(rvm_cfg_t* cfg, void *buf)
{
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx+=2)
    {
        blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == buf)
            break;
    }

    if(bx == BLOCK_TBL_SIZE) {
        rvm_log("Couldn't find buf in rvm block table\n");
        errno = EINVAL;
        return false;
    }

    /* Cleanup remote info */
    rmem_free(&cfg->rmem, bx + 1); 
    
    LOG(5, ("rmem_free id: %d\n", bx + 2));
    rmem_free(&cfg->rmem, bx + 2); // free shadow block as well
    if(blk->mr)
        ibv_dereg_mr(blk->mr);

    /* Free local info */
    /* TODO We don't really free the entry in the block table. I just don't
     * want to deal with fragmentation yet.
     */
    rvm_unprotect(blk->local_addr, blk->size);
    free(blk->local_addr);
    blk->local_addr = NULL;

    cfg->blk_tbl->n_blocks -= 2;
    assert(cfg->blk_tbl->n_blocks >= 0);

    return true;
}

void *rvm_rec(rvm_cfg_t *cfg)
{
    static int64_t bx = 0;

    for(; bx < BLOCK_TBL_SIZE; bx+=2) {
        block_desc_t* blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr != NULL)
            break;
    }
    
    if(bx == BLOCK_TBL_SIZE)
        return NULL;

    void *res = cfg->blk_tbl->tbl[bx].local_addr;
    bx += 2;

    return res;
}

void block_write_sighdl(int signum, siginfo_t *siginfo, void *uctx)
{
    //XXX
    fprintf(stderr, "IN HANDLER: %lx\n", (uint64_t)siginfo->si_addr);
    if(in_sighdl == true) {
        /* Recursive segfault probably means this is a real fault. Restore the
         * default handler and proceed. Note, this may mess with debuggers. */
        signal(SIGSEGV, SIG_DFL);
        return;
    }
    in_sighdl = true;

    if(!cfg_glob->in_txn) {
        /* Not allowed to change pages outside of a txn. Issue a warning. */
        LOG(1, ("Recoverable page modified outside a txn\n"));
    }

    /* Check if the attempted read was for a recoverable page */
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++)
    {
        blk = &(cfg_glob->blk_tbl->tbl[bx]);
        if(blk->local_addr <= siginfo->si_addr 
           && blk->local_addr + blk->size > siginfo->si_addr)
            break;
    }

    if(bx == BLOCK_TBL_SIZE) {
        /* Address not in block table, this must be a real segfault */
        fprintf(stderr, "can't find block: %lx\n", (uint64_t)siginfo->si_addr);
            signal(SIGSEGV, SIG_DFL);
        return;
    }

    /* Found a valid block, mark it in the change list and unprotect.
     * Strictly speaking, this isn't legal because mprotect may not be reentrant
     * but in practice it should be fine. */
    BITSET(blk_chlist, bx);
    rvm_unprotect(siginfo->si_addr, _SC_PAGESIZE);

    in_sighdl = false;
    return;
}
