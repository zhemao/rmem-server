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
    rmem_layer_t* rmem_layer = (rmem_layer_t*)cfg->rmem_layer;

    /* Recover the block table */
    for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
    {
        /* Register with rmem */
        void *rec_tmp = rmem_layer->register_data(rmem_layer,
            (void*)cfg->blk_tbl.rbtbl + i*cfg->blk_sz, cfg->blk_sz);
        CHECK_ERROR(rec_tmp == NULL, ("Failed to register page %ld "
                   "of the block table with rmem\n", i));

        /* Fetch the block table page from server */
        err = rmem_layer->get(rmem_layer,
                (((void*)cfg->blk_tbl.rbtbl) + cfg->blk_sz*i),
                rec_tmp,
                BLK_REAL_TAG(BLOCK_TBL_ID + i), cfg->blk_sz);
        CHECK_ERROR(err != 0,
                ("Failed to recover page %ld of the block table\n", i));

        /* Fill in registration info */
        cfg->blk_tbl.rbtbl->tbl[BLOCK_TBL_ID + i].blk_rec = rec_tmp;
    }

    /* Recover every previously allocated block. Start reading the block table
     * after the entries describing itself (those were recovered above). */
    int bx;
    for(bx = BLOCK_TBL_NPG; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->bid < 0)
            continue; //Freed memory

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
                (struct ibv_mr*)blk->blk_rec, BLK_REAL_TAG(blk->bid), cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = EUNKNOWN;
            return false;
        }

        /* Protect the block to detect changes */
        rvm_protect(blk->local_addr, cfg->blk_sz);

        LOG(9, ("Recovered block %d (shadow %d) - local addr: %p\n",
                    blk->bid, BLK_SHDW_TAG(blk->bid), blk->local_addr));
    }

    /* Protect the block table to prevent further changes */
    rvm_protect(cfg->blk_tbl.rbtbl, BLOCK_TBL_SIZE);

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

    if(opts->recovery) {
        if(!recover_blocks(cfg))
            return NULL;

        /* Rebuild the local block table information */
        res = btbl_init(&(cfg->blk_tbl), cfg->blk_tbl.rbtbl);
        CHECK_ERROR(res == false, ("Failed to rebuild block table index\n"));

    } else {
        uint32_t tags[BLOCK_TBL_NPG*2];
        uint64_t addrs[BLOCK_TBL_NPG*2];

	    /* Initialize the raw block table (that will be preserved) */
        res = rbtbl_init(cfg->blk_tbl.rbtbl);
        CHECK_ERROR(res == false, ("Failed to initialize block table\n"));

        /* Initialize the local block table information */
        res = btbl_init(&(cfg->blk_tbl), cfg->blk_tbl.rbtbl);
        CHECK_ERROR(res == false, ("Failed to rebuild block table index\n"));

        for(size_t i = 0; i < BLOCK_TBL_NPG; i++) {
           /* Grab block descriptors for the block table itself. */
            blk_desc_t *blk = btbl_alloc(&(cfg->blk_tbl),
                    (void*)cfg->blk_tbl.rbtbl + i*cfg->blk_sz);
            CHECK_ERROR(blk == NULL, ("Failed to allocate block table space for "
                        "page %ld of the block table\n", i));

            /* Register the pages of the block table with rmem */
            blk->blk_rec = rmem_layer->register_data(rmem_layer,
                (void*)cfg->blk_tbl.rbtbl + i*cfg->blk_sz, cfg->blk_sz);
            CHECK_ERROR(blk->blk_rec == NULL, ("Failed to register page %ld "
                        "of the block table with rmem\n", i));

            /* Set up rmem_malloc info */
	        tags[i] = BLK_REAL_TAG(blk->bid);
	        tags[BLOCK_TBL_NPG + i] = BLK_SHDW_TAG(blk->bid);
        }

        /* Allocate and register the block table remotely */
        int ret = rmem_layer->multi_malloc(
            rmem_layer, addrs, cfg->blk_sz, tags, BLOCK_TBL_NPG*2);
        CHECK_ERROR(ret != 0, ("Failed to allocate memory for block table\n"));

#if (LOG_LEVEL > 9)
        for (size_t i = 0; i < BLOCK_TBL_NPG; i++)
        {
            LOG(9, ("Allocated block table page %ld - local addr: %p "
                    "- remote addr: %lx\n",
                BLOCK_TBL_ID + i, cfg->blk_tbl.rbtbl + i*cfg->blk_sz, addrs[i]));
        }
#endif

        /* Make an initial write of the block table */
        rvm_txn_commit(cfg, 0);

//        LOG(9, ("Commiting initial block table, (%ld blocks)\n", BLOCK_TBL_NPG));
//        for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
//        {
//            res = rmem_layer->put(rmem_layer, BLOCK_TBL_ID + i,
//                    (((void*)cfg->blk_tbl.rbtbl) + cfg->blk_sz*i),
//                    cfg->blk_tbl_rec, cfg->blk_sz);
//
//            if(res != 0) {
//                rvm_log("Failed to write block table\n");
//                return false;
//            }
//        }
    }

    /* A global config is used because signal handlers need access to it. */
    /* TODO: We probably don't need the cfg argument anymore since there can
     * only be one now. */
    cfg_glob = cfg;

    /* Install our special signal handler to track changed blocks */
    in_sighdl = false;
    struct sigaction sigact;
    sigact.sa_sigaction = block_write_sighdl;
    sigact.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&(sigact.sa_mask));
    sigaction(SIGSEGV, &sigact, NULL);

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

    LOG(9, ("tearing down configuration\n"));

    /* Free all remote blocks (leave local blocks)*/
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->bid < 0)
            continue;

        /* Free the remote blocks */
        rmem_layer->free(rmem_layer, BLK_REAL_TAG(blk->bid));
        rmem_layer->free(rmem_layer, BLK_SHDW_TAG(blk->bid));

        rmem_layer->deregister_data(rmem_layer, blk->blk_rec);

        /* Unprotect block */
        rvm_unprotect(blk->local_addr, cfg->blk_sz);
    }

    /* We need to commit in order for the frees to actually happen */
    rmem_layer->atomic_commit(rmem_layer, NULL, NULL, NULL, 0);
    rmem_layer->deregister_data(rmem_layer, cfg->blk_tbl_rec);
    rmem_layer->disconnect(rmem_layer);

    /* Free local memory */
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
    int err;
    rmem_layer_t *rmem_layer = cfg->rmem_layer;

    char *btbl_cpy = malloc(BLOCK_TBL_SIZE);
    RETURN_ERROR(btbl_cpy == NULL, false,
            ("Couldn't allocate a copy of the block table\n"));

    struct ibv_mr* block_mr = (struct ibv_mr*)rmem_layer->register_data(
            rmem_layer, btbl_cpy, BLOCK_TBL_SIZE);
    assert(block_mr != NULL);

    /* Recover the block table */
    for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
    {
        err = rmem_layer->get(rmem_layer,
                (btbl_cpy + cfg->blk_sz*i),
                block_mr,
                BLOCK_TBL_ID + 2 * i, cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block table\n");
            errno = EUNKNOWN;
            return false;
        }
    }

    /* Check if block table looks right */
    err = memcmp(btbl_cpy, cfg->blk_tbl.rbtbl, BLOCK_TBL_SIZE);
    RETURN_ERROR(err != 0, false, ("Block table doesn't match replica\n"));

    /* Clean up block table copy */
    rmem_layer->deregister_data(rmem_layer, block_mr);
    free(btbl_cpy);
    
    /* Storage for copies of each block */
    char *blk_cpy = malloc(cfg->blk_sz);
    assert(blk_cpy != NULL);
    block_mr = rmem_layer->register_data(rmem_layer, blk_cpy, cfg->blk_sz);
    assert(block_mr != NULL);

    /* Recover every previously allocated block */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);
        if(blk->bid < 0)
            continue; //Freed memory

        assert(blk->local_addr != NULL);

        /* Actual fetch from server */
        err = rmem_layer->get(rmem_layer, blk_cpy,
                block_mr, BLK_REAL_TAG(blk->bid),
                cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = EUNKNOWN;
            return false;
        }

        /* Check if server has the same version */
        err = memcmp(blk_cpy, blk->local_addr, cfg->blk_sz);
        RETURN_ERROR(err != 0, false, ("Block %d doesn't match replica\n", bx));
    }

    /* Clean up block copy storage */
    rmem_layer->deregister_data(rmem_layer, block_mr);
    free(blk_cpy);

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
//    LOG(9, ("Commiting block table, (%ld blocks)\n", BLOCK_TBL_NPG));
//    for(size_t i = 0; i < BLOCK_TBL_NPG; i++)
//    {
//        err = rmem_layer->put(rmem_layer, BLOCK_TBL_ID + i,
//                (((void*)cfg->blk_tbl.rbtbl) + cfg->blk_sz*i),
//                cfg->blk_tbl_rec, cfg->blk_sz);
//
//        if(err != 0) {
//            rvm_log("Failed to write block table\n");
//            errno = err;
//            return false;
//        }
//    }
    
    uint32_t tags_src[BLOCK_TBL_NENT];
    uint32_t tags_dst[BLOCK_TBL_NENT];
    uint32_t tags_size[BLOCK_TBL_NENT];
    int count = 0;

    /* Walk the block table and commit everything that's changed */
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(cfg->blk_tbl.rbtbl->tbl[bx]);

        /* If the block hasn't changed, skip it */
        if(!btbl_test_mod(&(cfg->blk_tbl), blk))
            continue;

        err = rmem_layer->put(rmem_layer, BLK_SHDW_TAG(blk->bid),
                blk->local_addr, blk->blk_rec, cfg->blk_sz);
        CHECK_ERROR(err != 0, ("Failed to write block %d", bx));

        tags_src[count] = BLK_SHDW_TAG(blk->bid);
        tags_dst[count] = BLK_REAL_TAG(blk->bid);
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

    uint32_t *tags = malloc(2 * nblocks * sizeof(uint32_t));
    uint64_t *addrs = malloc(2 * nblocks * sizeof(uint64_t));
    int tag_ind = 0;

    CHECK_ERROR(tags == NULL, ("Failure: alloc tag and addr buffers\n"));

    for(int b = 0; b < nblocks; b++)
    {
        blk_desc_t *block = btbl_alloc(&(cfg->blk_tbl), 
                start_addr + b*cfg->blk_sz);
        CHECK_ERROR(block == NULL, ("Couldn't find free block in table\n"));

        /* Allocate and register the block remotely */
        /* TODO Right now we pin everything, all the time. Eventually we may want
         * to have some caching thing where we only pin hot pages or something.
         */
        block->blk_rec = rmem_layer->register_data(
		rmem_layer, block->local_addr, cfg->blk_sz);

        if(block->blk_rec == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = EUNKNOWN;
            return NULL;
        }

        tags[tag_ind] = BLK_REAL_TAG(block->bid);
        tags[tag_ind + 1] = BLK_SHDW_TAG(block->bid);
        tag_ind += 2;

        LOG(9, ("Allocated block %d (shadow %d) - local addr: %p\n",
                    BLK_REAL_TAG(block->bid), BLK_SHDW_TAG(block->bid),
                    block->local_addr));
    }

    int ret = rmem_layer->multi_malloc(
	    rmem_layer, addrs, cfg->blk_sz, tags, 2 * nblocks);
    if (ret != 0) {
	rvm_log("Failed to allocate remote memory for blocks\n");
	errno = EUNKNOWN;
	return NULL;
    }

    free(tags);
    free(addrs);

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
    uint32_t tags[2];

    blk_desc_t *blk = btbl_lookup(&(cfg->blk_tbl), buf);

    LOG(9, ("rmem_free block: %d (%d) - local addr: %p\n",
                BLK_REAL_TAG(blk->bid), BLK_SHDW_TAG(blk->bid), buf));

    /* Cleanup remote info */
    tags[0] = BLK_REAL_TAG(blk->bid);
    tags[1] = BLK_SHDW_TAG(blk->bid);
    rmem_layer->multi_free(rmem_layer, tags, 2);
    rmem_layer->deregister_data(rmem_layer, blk->blk_rec);

    /* Unset the change bit for this block */
    btbl_clear_mod(&(cfg->blk_tbl), blk);

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
    btbl_mark_mod(&(cfg_glob->blk_tbl), blk);
    rvm_unprotect(page_addr, cfg_glob->blk_sz);

    in_sighdl = false;
    return;
}
