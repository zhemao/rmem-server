/*
 * block_table.c
 *
 *  Created on: May 8, 2015
 *      Author: violet
 */

#include <limits.h>
#include "block_table.h"

bool rbtbl_init(raw_blk_tbl_t *rbtbl)
{
    rbtbl->free = NULL;

    /*TODO Hack due to lazyness: block 0 is reserved because -0 is still
     * 0 so we can't tell if it's free or not. We just don't put it on the
     * free list */
    rbtbl->tbl[0].bid = INT_MIN;
    rbtbl->tbl[0].blk_rec = NULL;
    rbtbl->tbl[0].local_addr = NULL;

    /* Initialize all blocks to free. */
    for(int bx = 1; bx < BLOCK_TBL_NENT; bx++)
    {
        rbtbl->tbl[bx].bid = -bx;
        rbtbl->tbl[bx].blk_rec = NULL;

        /* Push onto free list */
        rbtbl->tbl[bx].local_addr = rbtbl->free;
        rbtbl->free = &(rbtbl->tbl[bx]);
    }
    
    return true;
}

bool btbl_init(blk_tbl_t *btbl, raw_blk_tbl_t *rbtbl)
{
    /* Allocate and initialize the block change list */
    btbl->blk_chlist = (bitmap_t*)malloc(BITNSLOTS(BLOCK_TBL_NENT)*sizeof(int32_t));
    memset(btbl->blk_chlist, 0, BITNSLOTS(BLOCK_TBL_NENT)*sizeof(int32_t));

    // Will eventually insert each allocated bdesc into an index

    return true;
}

/* TODO Were just doing a linear search right now, I'm sure we could do
 * something better.
 */
blk_desc_t *btbl_lookup(blk_tbl_t *btbl, void *target)
{
    blk_desc_t *blk;
    int bx;
    for(bx = 1; bx < BLOCK_TBL_NENT; bx++)
    {
        blk = &(btbl->rbtbl->tbl[bx]);
        if(blk->local_addr == target)
            break;
    }

    if(bx == BLOCK_TBL_NENT)
        return NULL;
    else
        return blk;
}

bool btbl_free(blk_tbl_t *tbl, blk_desc_t *desc)
{
    desc->bid = -(desc->bid);
    desc->local_addr = tbl->rbtbl->free;
    tbl->rbtbl->free = desc;
    tbl->rbtbl->n_blocks--;

    return true;
}

blk_desc_t *btbl_alloc(blk_tbl_t *tbl)
{
    /* Pop a descriptor off the free list */
    blk_desc_t *desc = tbl->rbtbl->free;
    if(desc == NULL) {
        return NULL;
    } else {
        desc->bid = -(desc->bid);
        tbl->rbtbl->free = (blk_desc_t*)desc->local_addr;
        tbl->rbtbl->n_blocks++;
        return desc;
    }
}
