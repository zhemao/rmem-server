/* Implementation of user-facing functions for rvm */
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
#include <limits.h>
#include "rvm.h"
#include "rvm_int.h"
#include "common.h"
#include "block_table.h"
#include "utils/log.h"
#include "utils/error.h"

static inline int rvm_protect(void *addr, size_t size)
{
    return mprotect(addr, size, PROT_READ | PROT_EXEC);
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
    bool res;
    rmem_layer_t* rmem_layer = (rmem_layer_t*)cfg->rmem_layer;

    /* Recover the block table */
    for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
    {
        err = rmem_layer->get(rmem_layer,
                (((void*)cfg->blk_tbl.rbtbl) + cfg->blk_sz*i),
                (struct ibv_mr*)cfg->blk_tbl_rec,
                BLOCK_TBL_ID + i, cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block table\n");
            errno = EUNKNOWN;
            return false;
        }
    }

    /* Recover every previously allocated block */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->bid < 0)
            continue; //Freed memory

        //XXX
        if(blk->local_addr == NULL)
            printf("NULL: bx %d. bid %d\n", bx, blk->bid);
        assert(blk->local_addr != NULL);

        /* Allocate local storage for recovered block */
        LOG(8, ("mmap in addr: %p\n", blk->local_addr));
        void *new_addr = mmap(blk->local_addr, cfg->blk_sz,
                (PROT_READ | PROT_WRITE | PROT_EXEC),
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
        blk->blk_rec = rmem_layer->register_data(
                rmem_layer, blk->local_addr, cfg->blk_sz);
        if(blk->blk_rec == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        /* Actual fetch from server */
        err = rmem_layer->get(rmem_layer, blk->local_addr,
                (struct ibv_mr*)blk->blk_rec, BLK_REAL_TAG(bx), cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = EUNKNOWN;
            return false;
        }

        /* Protect the block to detect changes */
        rvm_protect(blk->local_addr, cfg->blk_sz);

        LOG(9, ("Recovered block %d (shadow %ld) - local addr: %p\n",
                    blk->bid, BLK_SHDW_TAG(bx), blk->local_addr));
    }

    /* TODO Maybe this step should be integrated with the step above */
    res = btbl_rec(&(cfg->blk_tbl), cfg->blk_tbl.rbtbl);
    CHECK_ERROR(res == false, ("Failed to rebuild block table index\n"));

    return true;
}

rvm_cfg_t *rvm_cfg_create(rvm_opt_t *opts, create_rmem_layer_f create_rmem_layer_function)
{
    bool res;
    rvm_cfg_t *cfg = (rvm_cfg_t*)malloc(sizeof(rvm_cfg_t));
    if(cfg == NULL)
        return NULL;
    cfg->blk_sz = sysconf(_SC_PAGESIZE);
    cfg->in_txn = false;
    cfg->alloc_fp = opts->alloc_fp;
    cfg->free_fp = opts->free_fp;
    cfg->alloc_data = NULL;

    rmem_layer_t* rmem_layer = cfg->rmem_layer = create_rmem_layer_function();

    rmem_layer->connect(rmem_layer, opts->host, opts->port);

    /* Allocate and initialize the block table locally */
    cfg->blk_tbl.rbtbl = (raw_blk_tbl_t *)mmap(NULL, BLOCK_TBL_SIZE,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
    assert((size_t)cfg->blk_tbl.rbtbl % sysconf(_SC_PAGESIZE) == 0);

    if(cfg->blk_tbl.rbtbl == NULL) {
        errno = EUNKNOWN;
        return NULL;
    }

    /* Register the local block table with IB */
    cfg->blk_tbl_rec = rmem_layer->register_data(rmem_layer,
            cfg->blk_tbl.rbtbl, BLOCK_TBL_SIZE);
    if(cfg->blk_tbl_rec == NULL) {
        rvm_log("Failed to register memory for block table\n");
        errno = EUNKNOWN;
        return NULL;
    }

    /* Allocate and initialize the block change list */
    blk_chlist = (bitmap_t*)malloc(BITNSLOTS(BLOCK_TBL_NENT)*sizeof(int32_t));
    memset(blk_chlist, 0, BITNSLOTS(BLOCK_TBL_NENT)*sizeof(int32_t));

    if(opts->recovery) {
        if(!recover_blocks(cfg))
            return NULL;
    } else {

        res = rbtbl_init(cfg->blk_tbl.rbtbl);
        CHECK_ERROR(res == false, ("Failed to initialize block table\n"));

        /* TODO rmem should support arbitrarily sized allocations. I've avoided
         * it because I think eventually we'll have a backend with fixed-sized
         * allocations. */
        for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
        {
            /* Allocate and register the block table remotely */
            uint64_t ret = rmem_layer->malloc(rmem_layer, cfg->blk_sz,
                    BLOCK_TBL_ID + i);
            if(ret == 0) {
                rvm_log("Failed to register memory for block table\n");
                errno = EUNKNOWN;
                return NULL;
            }
            LOG(9, ("Allocated block %ld - local addr: %p\n",
                    BLOCK_TBL_ID + i, (void*)cfg->blk_tbl.rbtbl + i*cfg->blk_sz));
        }
        cfg->blk_tbl.rbtbl->n_blocks = 0;

        /* Do an initial commit of the block table */
        /*TODO this may not work if we make txids do something of of we start
         * treating the block table like any other block...*/
        LOG(9, ("Commiting initial block table\n"));
        rvm_txn_commit(cfg, 0);
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
    rmem_layer_t* rmem_layer = cfg->rmem_layer;

    if(cfg == NULL) {
        rvm_log("Received Null configuration\n");
        errno = EINVAL;
        return false;
    }

    /* Free all remote blocks (leave local blocks)*/
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx+=2)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue;

        /* Free the remote blocks */
        rmem_layer->free(rmem_layer, BLK_REAL_TAG(bx)); // free 'real' block
        rmem_layer->free(rmem_layer, BLK_SHDW_TAG(bx)); // free 'shadow' block
        
        rmem_layer->deregister_data(rmem_layer, blk->blk_rec);

        /* Unprotect block */
        rvm_unprotect(blk->local_addr, cfg->blk_sz);
    }

    /* Clean up config info on server */
    rmem_layer->free(rmem_layer, BLOCK_TBL_ID);

    rmem_layer->deregister_data(rmem_layer, cfg->blk_tbl_rec);

    rmem_layer->disconnect(rmem_layer);

    /* Free local memory */
    free(blk_chlist);
    munmap(cfg->blk_tbl.rbtbl, cfg->blk_sz);
    free(cfg);

    return true;
}

size_t rvm_get_blk_sz(rvm_cfg_t *cfg)
{
    return cfg->blk_sz;
}

rvm_txid_t rvm_txn_begin(rvm_cfg_t* cfg)
{
    cfg->in_txn = true;

    //Always return the same txid because we don't support nesting or rollback
    return 1;
}

bool check_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid)
{
    rmem_layer_t* rmem_layer = cfg->rmem_layer;
    LOG(8, ("Checking txn commit\n"));

    // check block table
    char* block = (char*)malloc(sizeof(char) * cfg->blk_sz);
    RETURN_ERROR(block == 0, false,
            ("Failure: Error allocating table\n"));

    struct ibv_mr* block_mr = (struct ibv_mr*)rmem_layer->register_data(rmem_layer, block, sizeof(char) * cfg->blk_sz);

    LOG(8, ("Getting block table\n"));
    int err = rmem_layer->get(rmem_layer, block, block_mr, BLOCK_TBL_ID, cfg->blk_sz);
    LOG(8, ("Block table fetched\n"));

    RETURN_ERROR(err != 0, false,
            ("Failure: Error doing RDMA read of block table\n"));

    err = memcmp(block, cfg->blk_tbl.rbtbl, sizeof(char) * cfg->blk_sz);

    RETURN_ERROR(err != 0, false,
            ("Block table does not match replica\n"));

    //check data blocks
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++) {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->local_addr == NULL) // ?
            continue;

        char* block = (char*)malloc(sizeof(char) * cfg->blk_sz);
        struct ibv_mr* block_mr = (struct ibv_mr*)rmem_layer->register_data(
                rmem_layer, block, sizeof(char) * cfg->blk_sz);

        int err = rmem_layer->get(rmem_layer, block, block_mr,
                BLK_REAL_TAG(bx), cfg->blk_sz);
        RETURN_ERROR(err != 0, false,
            ("Failure: Error doing RDMA read of block\n"));
        

        err = memcmp(block, blk->local_addr, sizeof(char) * cfg->blk_sz);
        RETURN_ERROR(err != 0, false,
                ("Block %d does not match replica\n", bx));

        free(block);

    }

    free(block);
    rmem_layer->deregister_data(rmem_layer, block_mr);

    cfg->in_txn = false;

    return true;
}

bool rvm_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid)
{
    LOG(5, ("TXN Commit\n"));

    /* TODO This is a brain-dead initial implementation. We simply copy over
     * everything in the block table, even if it wasn't modified. The long-term
     * solution is to use protection bits on the page table to automatically
     * detect changes and only copy those.
     */
    int err;
    rmem_layer_t* rmem_layer = cfg->rmem_layer;

    /* Copy the updated block table */
    /* TODO We shouldn't have to do this here, the block table should describe
     * itself and get updated the same way anything else is */
    LOG(9, ("Commiting block table, (%ld blocks)\n", BLOCK_TBL_NPG));
    for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
    {
        err = rmem_layer->put(rmem_layer, BLOCK_TBL_ID + i,
                (((void*)cfg->blk_tbl.rbtbl) + cfg->blk_sz*i),
                cfg->blk_tbl_rec, cfg->blk_sz);

        if(err != 0) {
            rvm_log("Failed to write block table\n");
            errno = err;
            return false;
        }
    }
    
    uint32_t tags_src[BLOCK_TBL_NENT];
    uint32_t tags_dst[BLOCK_TBL_NENT];
    uint32_t tags_size[BLOCK_TBL_NENT];
    int count = 0;

    /* Walk the block table and commit everything */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        /* If the block hasn't changed, skip it */
        if(!BITTEST(blk_chlist, bx))
            continue;

        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        err = rmem_layer->put(rmem_layer, BLK_SHDW_TAG(bx),
                blk->local_addr, blk->blk_rec, cfg->blk_sz);
        CHECK_ERROR(err != 0, ("Failed to write block %d", bx));

        tags_src[count] = BLK_SHDW_TAG(bx);
        tags_dst[count] = BLK_REAL_TAG(bx);
        tags_size[count++] = cfg->blk_sz;

        /* Re-protect the block for the next txn */
        rvm_protect(blk->local_addr, cfg->blk_sz);
    }

    int ret = rmem_layer->atomic_commit(
            rmem_layer, tags_src, tags_dst, tags_size, count);
    CHECK_ERROR(ret != 0, ("Failure: atomic commit\n"));

    return true;
}

bool rvm_set_alloc_data(rvm_cfg_t *cfg, void *alloc_data)
{
    cfg->blk_tbl.rbtbl->alloc_data = alloc_data;
        return true;
}

void *rvm_get_alloc_data(rvm_cfg_t *cfg)
{
    return cfg->blk_tbl.rbtbl->alloc_data;
}

void *rvm_alloc(rvm_cfg_t *cfg, size_t size)
{
    return cfg->alloc_fp(cfg, size);
}

/* Can now allocate more than one page. It still allocates in multiples of the
 * page size. If you want better functionality, you can implement a library on
 * top, for instance buddy_alloc.h. Internally it still thinks in terms of
 * pages, so if you allocate 4 pages, then there will be 4 entries in the block
 * table.
 * TODO: Still uses a fixed-size block-table so there is a cap on the number
 * of allocations. */
void *rvm_blk_alloc(rvm_cfg_t* cfg, size_t size)
{
    LOG(6, ("Block Alloc of size %ld\n", size));

    rmem_layer_t* rmem_layer = cfg->rmem_layer;

    if (size == 0) {
        return NULL;
    }

    /* Allocate and initialize the block locally */
    if(cfg->blk_tbl.rbtbl->n_blocks == BLOCK_TBL_NENT) {
        //Out of room in the block table
        rvm_log("Block table out of space\n");
        errno = ENOMEM;
        return NULL;
    }

    /* Number of blocks is the ceiling of size / block_size */
    size_t nblocks = INT_DIV_CEIL(size, cfg->blk_sz);

    /* Allocate local memory for this region.
     * Note: It's important to allocate an integer number of blocks instead of
     * just using size. This is because we protect whole pages. */
    void *start_addr;
    start_addr = mmap(NULL, nblocks*cfg->blk_sz, 
            (PROT_READ | PROT_WRITE | PROT_EXEC), 
            (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0);
    assert((size_t)start_addr % sysconf(_SC_PAGESIZE) == 0);

    if(start_addr == NULL) {
        LOG(8, ("Failed to allocate local memory for block\n"));
        errno = EUNKNOWN;
        return NULL;
    }

    //for(bx = b_start; bx < b_start + nblocks; bx++)
    for(int b = 0; b < nblocks; b++)
    {
        blk_desc_t *block = btbl_alloc(&(cfg->blk_tbl));
        CHECK_ERROR(block == NULL, ("Couldn't find free block in table\n"));

        /* Calculate the start address of each block in the region */
        block->local_addr = start_addr + b*cfg->blk_sz;

        /* Allocate and register the block remotely */
        /* TODO Right now we pin everything, all the time. Eventually we may want
         * to have some caching thing where we only pin hot pages or something.
         */
        block->blk_rec = rmem_layer->register_data(rmem_layer, block->local_addr, cfg->blk_sz);
        if(block->blk_rec == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }
        uintptr_t real_ptr = rmem_layer->malloc(rmem_layer, cfg->blk_sz,
                BLK_REAL_TAG(block->bid));

        // allocate shadow block
        uintptr_t shadow_ptr = rmem_layer->malloc(rmem_layer, cfg->blk_sz,
                BLK_SHDW_TAG(block->bid));
        if(real_ptr == 0 || shadow_ptr == 0) {
            rvm_log("Failed to allocate remote memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        LOG(9, ("Allocated block %ld (shadow %ld) - local addr: %p\n",
                    BLK_REAL_TAG(block->bid), BLK_SHDW_TAG(block->bid),
                    block->local_addr));
    }

    /* Protect the local blocks so that we can keep track of changes */
    rvm_protect(start_addr, nblocks*cfg->blk_sz);

    return start_addr;
}

bool rvm_free(rvm_cfg_t *cfg, void *buf)
{
    return cfg->free_fp(cfg, buf);
}

bool rvm_blk_free(rvm_cfg_t* cfg, void *buf)
{
    bool res;
    rmem_layer_t* rmem_layer = cfg->rmem_layer;

    blk_desc_t *blk = btbl_lookup(&(cfg->blk_tbl), buf);

    LOG(9, ("rmem_free block: %ld (%ld) - local addr: %p\n",
                BLK_REAL_TAG(blk->bid), BLK_SHDW_TAG(blk->bid), buf));

    /* Cleanup remote info */
    rmem_layer->free(rmem_layer, BLK_REAL_TAG(blk->bid));
    rmem_layer->free(rmem_layer, BLK_SHDW_TAG(blk->bid));
    rmem_layer->deregister_data(rmem_layer, blk->blk_rec);

    /* Unset the change bit for this block */
    BITCLEAR(blk_chlist, blk->bid);

    /* Free local info */
    rvm_unprotect(blk->local_addr, cfg->blk_sz);
    munmap(blk->local_addr, cfg->blk_sz);
    blk->local_addr = NULL;

    /* free in the block table */
    res = btbl_free(&(cfg->blk_tbl), blk);
    CHECK_ERROR(res == false, ("Failed to free block in block table\n"));

    return true;
}

/** Set the user's private data. Pointer "data" will be available after
 *  recovery.
 *
 */
bool rvm_set_usr_data(rvm_cfg_t *cfg, void *data)
{
    cfg->blk_tbl.rbtbl->usr_data = data;
    return true;
}

void *rvm_get_usr_data(rvm_cfg_t *cfg)
{
    return cfg->blk_tbl.rbtbl->usr_data;
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

    if(siginfo->si_addr == NULL) {
        LOG(1, ("NULL Pointer Reference\n"));
        signal(SIGSEGV, SIG_DFL);
        return;
    }

    if(!cfg_glob->in_txn) {
        /* Not allowed to change pages outside of a txn. Issue a warning. */
        LOG(5, ("Recoverable page modified outside a txn\n"));
    }

    void *page_addr = (void*)(((uint64_t)siginfo->si_addr / cfg_glob->blk_sz) *
        cfg_glob->blk_sz);

    /* Check if the attempted read was for a recoverable page */
    blk_desc_t *blk = btbl_lookup(&(cfg_glob->blk_tbl), page_addr);

    if(blk == NULL) {
        /* Address not in block table, this must be a real segfault */
        fprintf(stderr, "can't find block: %p\n", page_addr);
            signal(SIGSEGV, SIG_DFL);
        return;
    }

    /* Found a valid block, mark it in the change list and unprotect. */
    /* Strictly speaking, this isn't legal because mprotect may not be reentrant
     * but in practice it should be fine. */
    BITSET(blk_chlist, blk->bid);
    rvm_unprotect(page_addr, cfg_glob->blk_sz);

    in_sighdl = false;
    return;
}
