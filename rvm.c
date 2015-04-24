/* Implementation of user-facing functions for rvm */
#include <unistd.h>
#include <errno.h>
#include "rvm.h"
#include "rvm_int.h"
#include "rmem.h"

/* Recover the block table and all pages
 * TODO Right now this just loads everything into new locations. Eventually this
 * will need to re-write pointers or map stuff to the original address.*/
static bool recover_blocks(rvm_cfg_t *cfg)
{
    int err;

    /* Recover the block table */
    uint64_t raddr = rmem_lookup(&(cfg->rmem), BLOCK_TBL_ID);
    err = rmem_get(&(cfg->rmem), cfg->blk_tbl, cfg->blk_tbl_mr, raddr, cfg->blk_sz);
    if(err != 0) {
        rvm_log("Failed to recover block table\n");
        errno = 0;
        return false;
    }

    /* Recover every previously allocated block */
    for(int bx = 0; bx < cfg->blk_tbl->end; bx++)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue; //Freed memory

        /* Allocate local storage for recovered block */
        err = posix_memalign(&(blk->local_addr), cfg->blk_sz, cfg->blk_sz);
        if(err != 0) {
            errno = err;
            return false;
        }

        /* Register new memory with IB */
        blk->mr = rmem_create_mr(blk->local_addr, cfg->blk_sz);
        if(blk->mr == NULL) {
            rvm_log("Failed to register memory for block\n");
            errno = 0;
            return NULL;
        }

        /* Actual fetch from server */
        err = rmem_get(&(cfg->rmem), blk->local_addr, blk->mr,
                blk->raddr, cfg->blk_sz);
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
    err = posix_memalign(&(cfg->blk_tbl), cfg->blk_sz, cfg->blk_sz);
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
    for(int bx = 0; bx < cfg->blk_tbl->end; bx++)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue;

        rmem_free(&rmem, blk->raddr);
        if(blk->mr)
            ibv_dereg_mr(blk->mr);
    }

    /* Clean up config info on server */
    rmem_free(cfg->rmem, cfg->blk_tbl->raddr);
    ibv_dereg_mr(cfg->blk_tbl->mr);
    rmem_disconnect(&(cfg->rmem));

    /* Free local memory */
    free(cfg->blk_tbl);
    free(cfg);

    return true;
}

rvm_txid_t rvm_txn_begin(rvm_cfg_t *cfg)
{
    //Nothing to do yet

    //Always return the same txid because we don't support nesting or rollback
    return 1;
}

bool rvm_txn_commit(rvm_cfg_t *cfg, rvm_txid_t txid)
{
    /* TODO This is a brain-dead initial implementation. We simply copy over
     * everything in the block table, even if it wasn't modified. The long-term
     * solution is to use protection bits on the page table to automatically
     * detect changes and only copy those.
     *
     * TODO This also is not atomic. The current plan is to add a simple
     * transaction to the rmem_put API.
     */
    int err;

    /* Copy the updated block table */
    err = rmem_put(&(cfg->rmem), cfg->blk_tbl->raddr, cfg->blk_tbl,
            cfg->blk_tbl->mr, cfg->blk_sz);
    if(err != 0) {
        rvm_log("Failed to write block table\n");
        errno = err;
        return false;
    }

    /* Walk the block table and commit everything */
    for(int bx = 0; bx < cfg->blk_tbl->end; bx++)
    {
        block_desc_t *blk = &(cfg->blk_tbl->tbl[bx]);
        if(blk->local_addr == NULL)
            continue;

        /* TODO Eventually we may have to unpin blocks, this will catch that.
         * Right now this should never trigger. */
        assert(blk->mr != NULL);

        err = rmem_put(&(cfg->rmem), blk->raddr, blk->local_addr, blk->mr,
                cfg->blk_sz);
        if(err != 0) {
            rvm_log("Failed to write block %d\n", bx);
            errno = err;
            return false;
        }
    }

    return true;
}

void *rvm_alloc(rvm_cfg_t *cfg, size_t size)
{
    int err;

    /* TODO This is a brain-dead initial implementation, we allocate an entire
     * page for each allocation. Obviously this is wasteful, plus it caps the
     * max allocation at one page. This is just to get a basic working
     * implementation and will be improved later.
     */
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
    block->bid = BLOCK_TBL_ID + cfg->blk_tbl->end;
    cfg->blk_tbl->end++;
    err = posix_memalign(&(block->local_addr), cfg->blk_sz, cfg->blk_sz);
    if(err != 0) {
        errno = err;
        return NULL;
    }

    /* Allocate and register the block table remotely */
    /* TODO Right now we pin everything, all the time. Eventually we may want
     * to have some caching thing where we only pin hot pages or something.
     */
    block->mr = rmem_create_mr(block->local_addr, cfg->blk_sz);
    if(block->mr == NULL) {
        rvm_log("Failed to register memory for block\n");
        errno = 0;
        return NULL;
    }

    block->raddr = rmem_malloc(&(cfg->rmem), cfg->blk_sz, block->bid);
    if(block->raddr == 0) {
        rvm_log("Failed to allocate remote memory for block\n");
        errno = 0;
        return NULL;
    }

    return block->local_addr;
}

bool rvm_free(rvm_cfg_t *cfg, void *buf)
{
    /* TODO Were just doing a linear search right now, I'm sure we could do
     * something better.
     */
    block_desc_t *blk;
    int bx;
    for(bx = 0; bx < cfg->blk_tbl->end; bx++)
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
    rmem_free(&rmem, blk->raddr);
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

    void *res;
    while(bx < cfg->blk_tbl->end)
    {
        res = cfg->blk_tbl->tbl[bx].local_addr;
        bx++;
        if(res == NULL) {
            continue;
        } else {
            return res;
        }
    }

    return NULL;
}
