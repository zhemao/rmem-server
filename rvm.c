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
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++)
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
        blk->mr = rmem_create_mr(blk->local_addr, cfg->blk_sz);
        if(blk->mr == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        /* Actual fetch from server */
        err = rmem_get(&(cfg->rmem), blk->local_addr, blk->mr,
                BLK_REAL_TAG(bx), cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = EUNKNOWN;
            return false;
        }

        /* Protect the block to detect changes */
        rvm_protect(blk->local_addr, cfg->blk_sz);
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
        rvm_unprotect(blk->local_addr, cfg->blk_sz);
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
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++) {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL) // ?
            continue;

        /* TODO Eventually we may have to unpin blocks, this will catch that.
         * Right now this should never trigger. */
        assert(blk->mr != NULL);

        char* block = (char*)malloc(sizeof(char) * cfg->blk_sz);
        struct ibv_mr* block_mr = rmem_create_mr(block,
                sizeof(char) * cfg->blk_sz);

        int err = rmem_get(&(cfg->rmem), block, block_mr,
                BLK_REAL_TAG(bx), cfg->blk_sz);
        RETURN_ERROR(err != 0, false,
            ("Failure: Error doing RDMA read of block\n"));
        

        err = memcmp(block, blk->local_addr, sizeof(char) * cfg->blk_sz);
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

        /* Copy local block to it's shadow page */
        err = rmem_put(&(cfg->rmem), BLK_SHDW_TAG(bx), blk->local_addr, blk->mr,
                cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to write block %d\n", bx);
            errno = err;
            return false;
        }

        int ret = rmem_multi_cp_add(&cfg->rmem, BLK_REAL_TAG(bx),
                BLK_SHDW_TAG(bx), cfg->blk_sz);
        CHECK_ERROR(ret != 0,
                ("Failure: rmem_multi_cp_add\n"));

        /* Re-protect the block for the next txn */
        rvm_protect(blk->local_addr, cfg->blk_sz);
    }

    int ret = rmem_multi_cp_go(&cfg->rmem);
        CHECK_ERROR(ret != 0,
                ("Failure: rmem_multi_cp_go\n"));

    return true;
}

/* Can now allocate more than one page. It still allocates in multiples of the
 * page size. If you want better functionality, you can implement a library on
 * top, for instance buddy_alloc.h. Internally it still thinks in terms of
 * pages, so if you allocate 4 pages, then there will be 4 entries in the block
 * table.
 * TODO: Still uses a fixed-size block-table so there is a cap on the number
 * of allocations. */
void *rvm_alloc(rvm_cfg_t* cfg, size_t size)
{
    int err;

    if (size == 0) {
        return NULL;
    }

    /* Allocate and initialize the block locally */
    if(cfg->blk_tbl->n_blocks == BLOCK_TBL_SIZE) {
        //Out of room in the block table
        rvm_log("Block table out of space\n");
        errno = ENOMEM;
        return NULL;
    }

    /* Number of blocks is the ceiling of size / block_size */
    size_t nblocks = INT_DIV_CEIL(size, cfg->blk_sz);
    //XXX
    printf("nblocks: %ld\n", nblocks);

    // search for a run of free blocks big enough for this size
    int bx;
    size_t cur_run = 0; //Number of contiguous free blocks seen
    size_t b_start = 0; //Starting block of the current run
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++) 
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL) {
            cur_run++;
            if(cur_run == nblocks)
                break;
        } else {
            cur_run = 0;
            b_start = bx + 1;
        }
    }
    //XXX
    printf("Found slot: %ld\n", b_start);
    CHECK_ERROR(bx >= BLOCK_TBL_SIZE,
            ("Failure: Block table is full and we should have caught this earlier\n"));


    /* Allocate local memory for this region.
     * Note: It's important to allocate an integer number of blocks instead of
     * just using size. This is because we protect whole pages. */
    void *start_addr;
    err = posix_memalign(&start_addr, cfg->blk_sz, nblocks*cfg->blk_sz);
    if(err != 0) {
        errno = err;
        LOG(8, ("Failed to allocate local memory for block\n"));
        return NULL;
    }

    //XXX
    printf("Base addr: %p\n", start_addr);

    for(bx = b_start; bx < b_start + nblocks; bx++)
    {
        block_desc_t *block = &(cfg->blk_tbl->tbl[bx]);

        // ----- SHADOW PAGES ------ //
        // final destination of a block has block_id = X (odd numbers, starts at 1)
        // corresponding shadow block has block_id = X + 1
        block->bid = BLK_REAL_TAG(bx);
        cfg->blk_tbl->n_blocks++;

        /* Calculate the start address of each block in the region */
        block->local_addr = start_addr + (bx - b_start)*cfg->blk_sz;
        //XXX
        printf("Local addr: %p\n", block->local_addr);

        /* Allocate and register the block table remotely */
        /* TODO Right now we pin everything, all the time. Eventually we may want
         * to have some caching thing where we only pin hot pages or something.
         */
        block->mr = rmem_create_mr(block->local_addr, cfg->blk_sz);
        if(block->mr == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        // allocate final block
        //XXX
        printf("rmem_malloc: bid %d\n", block->bid);
        printf("rmem_malloc: bid %d\n", BLK_SHDW_TAG(bx));
        block->raddr = rmem_malloc(&(cfg->rmem), cfg->blk_sz, block->bid);
        // allocate shadow block
        uintptr_t shadow_ptr = rmem_malloc(&(cfg->rmem), cfg->blk_sz,
                BLK_SHDW_TAG(bx));
        if(block->raddr == 0 || shadow_ptr == 0) {
            rvm_log("Failed to allocate remote memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }
    }

    /* Protect the local blocks so that we can keep track of changes */
    rvm_protect(start_addr, nblocks*cfg->blk_sz);

    return start_addr;
}

bool rvm_free(rvm_cfg_t* cfg, void *buf)
{
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++)
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
    rmem_free(&cfg->rmem, BLK_REAL_TAG(bx)); 
    
    LOG(5, ("rmem_free id: %d\n", BLK_SHDW_TAG(bx)));
    rmem_free(&cfg->rmem, BLK_SHDW_TAG(bx)); // free shadow block as well
    if(blk->mr)
        ibv_dereg_mr(blk->mr);

    /* Free local info */
    /* TODO We don't really free the entry in the block table. I just don't
     * want to deal with fragmentation yet.
     */
    rvm_unprotect(blk->local_addr, cfg->blk_sz);
    free(blk->local_addr);
    blk->local_addr = NULL;

    cfg->blk_tbl->n_blocks--;
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
    if(in_sighdl == true) {
        /* Recursive segfault probably means this is a real fault. Restore the
         * default handler and proceed. Note, this may mess with debuggers. */
        LOG(1, ("Recursive segfault, restoring default handler\n"));
        signal(SIGSEGV, SIG_DFL);
        return;
    }
    in_sighdl = true;

    if(!cfg_glob->in_txn) {
        /* Not allowed to change pages outside of a txn. Issue a warning. */
        LOG(1, ("Recoverable page modified outside a txn\n"));
    }

    void *page_addr = (void*)(((uint64_t)siginfo->si_addr / cfg_glob->blk_sz) *
        cfg_glob->blk_sz);

    /* Check if the attempted read was for a recoverable page */
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < BLOCK_TBL_SIZE; bx++)
    {
        blk = &(cfg_glob->blk_tbl->tbl[bx]);
        if(blk->local_addr == page_addr)    
            break;
    }

    if(bx == BLOCK_TBL_SIZE) {
        /* Address not in block table, this must be a real segfault */
        fprintf(stderr, "can't find block: %p\n", page_addr);
            signal(SIGSEGV, SIG_DFL);
        return;
    }

    /* Found a valid block, mark it in the change list and unprotect. */
    /* Strictly speaking, this isn't legal because mprotect may not be reentrant
     * but in practice it should be fine. */
    BITSET(blk_chlist, bx);
    rvm_unprotect(page_addr, cfg_glob->blk_sz);

    in_sighdl = false;
    return;
}
