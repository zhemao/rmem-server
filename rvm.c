/* Implementation of user-facing functions for rvm */
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "rvm.h"
#include "rvm_int.h"
#include "rmem.h"
#include "common.h"
#include "log.h"
#include "error.h"

/* Recover the block table and all pages
 * TODO Right now this just loads everything into new locations. Eventually this
 * will need to re-write pointers or map stuff to the original address.*/
static bool recover_blocks(rvm_cfg_t *cfg)
{
    int err;

    /* Recover the block table */
    err = rmem_get(&(cfg->rmem), cfg->blk_tbl, cfg->blk_tbl_mr, BLOCK_TBL_ID, cfg->blk_sz);
    if(err != 0) {
        rvm_log("Failed to recover block table\n");
        errno = 0;
        return false;
    }

    /* Recover every previously allocated block */
    int bx;
    for(bx = 0; bx < cfg->blk_tbl->end; bx+=2)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue; //Freed memory

        /* Allocate local storage for recovered block */
        err = posix_memalign(&(blk->local_addr), cfg->blk_sz, blk->size);
        if(err != 0) {
            errno = err;
            return false;
        }

        /* Register new memory with IB */
        blk->mr = rmem_create_mr(blk->local_addr, blk->size);
        if(blk->mr == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = 0;
            return NULL;
        }

        /* Actual fetch from server */
        err = rmem_get(&(cfg->rmem), blk->local_addr, blk->mr,
                bx + 1, blk->size);
        if(err != 0) {
            rvm_log("Failed to recover block %d\n", bx);
            errno = 0;
            return false;
        }
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

    rmem_connect(&(cfg->rmem), opts->host, opts->port);

    /* Allocate and initialize the block table locally */
    err = posix_memalign((void**)&(cfg->blk_tbl), cfg->blk_sz, cfg->blk_sz);
    if(err != 0) {
        errno = err;
        return NULL;
    }

    /* Register the local block table with IB */
    cfg->blk_tbl_mr = rmem_create_mr(cfg->blk_tbl, cfg->blk_sz);
    if(cfg->blk_tbl_mr == NULL) {
        rvm_log("Failed to register memory for block table\n");
        errno = 0;
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
            errno = 0;
            return NULL;
        }

        cfg->blk_tbl->end = 0;
    }

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
    for(bx = 0; bx < cfg->blk_tbl->end; bx+=2) 
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue;

        rmem_free(&cfg->rmem, bx + 1); // free 'real' block
        rmem_free(&cfg->rmem, bx + 2 ); // free 'shadow' block
        if(blk->mr)
            ibv_dereg_mr(blk->mr);
    }

    /* Clean up config info on server */
    rmem_free(&cfg->rmem, BLOCK_TBL_ID);
    ibv_dereg_mr(cfg->blk_tbl_mr);
    rmem_disconnect(&(cfg->rmem));

    /* Free local memory */
    free(cfg->blk_tbl);
    free(cfg);

    return true;
}

rvm_txid_t rvm_txn_begin(rvm_cfg_t* cfg)
{
    //Nothing to do yet

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
    for(bx = 0; bx < cfg->blk_tbl->end; bx+=2) {
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
                ("Block %d does not match replica\n", bx));

        err = memcmp(block, blk->local_addr, sizeof(char) * blk->size);
        free(block);

    }

    ibv_dereg_mr(block_mr);

    return true;
}

//int rmem_get(struct rmem *rmem, void *dst, struct ibv_mr *dst_mr,
//        uint64_t src, size_t size)

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
    for(bx = 0; bx < cfg->blk_tbl->end; bx++)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
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
    if(cfg->blk_tbl->end == BLOCK_TBL_SIZE) {
        //Out of room in the block table
        rvm_log("Block table out of space\n");
        errno = ENOMEM;
        return NULL;
    }

    block_desc_t *block = &(cfg->blk_tbl->tbl[cfg->blk_tbl->end]);

    // set block size
    block->size = size;

    // ----- SHADOW PAGES ------ //
    // final destination of a block has block_id = X (odd numbers, starts at 1)
    // corresponding shadow block has block_id = X + 1
    block->bid = BLOCK_TBL_ID + cfg->blk_tbl->end + 1;
    cfg->blk_tbl->end += 2;
    err = posix_memalign(&(block->local_addr), cfg->blk_sz, size);
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
        errno = 0;
        return NULL;
    }

    // allocate final block
    block->raddr = rmem_malloc(&(cfg->rmem), size, block->bid);
    // allocate shadow block
    uintptr_t shadow_ptr = rmem_malloc(&(cfg->rmem), size, block->bid + 1);
    if(block->raddr == 0 || shadow_ptr == 0) {
        rvm_log("Failed to allocate remote memory for block\n");
        errno = 0;
        return NULL;
    }

    return block->local_addr;
}

bool rvm_free(rvm_cfg_t* cfg, void *buf)
{
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < cfg->blk_tbl->end; bx+=2)
    {
        blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == buf)
            break;
    }

    if(bx == cfg->blk_tbl->end) {
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
    free(blk->local_addr);
    blk->local_addr = NULL;

    return true;
}

void *rvm_rec(rvm_cfg_t *cfg)
{
    static int64_t bx = 0;
    if(bx == BLOCK_TBL_SIZE)
        return NULL;

    void *res = cfg->blk_tbl->tbl[bx].local_addr;
    bx+=2;

    return res;
}


