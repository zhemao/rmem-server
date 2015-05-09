/*
 * block_table.c
 *
 *  Created on: May 8, 2015
 *      Author: violet
 */

#include "block_table.h"

bool rbtl_init(raw_blk_tbl_t *rbtbl)
{
    rbtbl->free = NULL;

    /* Initialize all blocks to free. */
    for(int bx = 1; bx < BLOCK_TBL_NENT; bx++)
    {
        rbtbl->tbl[bx].bid = -bx;
        rbtbl->tbl[bx].blk_rec = NULL;

        /* Push onto free list */
        rbtbl->tbl[bx].local_addr = rbtbl->free;
        rbtbl->free = &(rbtbl->tbl[bx]);
    }
}

bool btbl_rec(blk_tbl_t *btbl, raw_blk_tbl_t *rbtbl)
{
    // Nothing to do yet, will eventually insert each
    // allocated bdesc into an index
    return true;
}

/* TODO Were just doing a linear search right now, I'm sure we could do
 * something better.
 */
int blk_tbl_lookup(blk_tbl_t *btbl, void *target)
{
    blk_desc_t *blk;
    int bx;
    for(bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk = &(btbl->rbtbl->tbl[bx]);
        if(blk->local_addr == target)
            break;
    }

    if(bx == BLOCK_TBL_NENT)
        return -1;
    else
        return bx;
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
    blk_desc_t *desc = tbl->rbtbl.free;
    if(desc == NULL) {
        return NULL;
    } else {
        desc->bid = -(desc->bid);
        tbl->rbtbl->free = (blk_desc_t*)desc->local_addr;
        tbl->rbtbl->n_blocks++;
        return desc;
    }
}
