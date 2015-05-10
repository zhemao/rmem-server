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
    rbtbl->n_blocks = 0;
    rbtbl->alloc_data = NULL;
    rbtbl->usr_data = NULL;

    /* Initialize all blocks to free.
     * Loop goes backward so that the free-list goes forward. This is important
     * because the block table takes the first BLOCK_TBL_NPG entries and those
     * must be in the first block. */
    for(int bx = BLOCK_TBL_NENT - 1; bx >= 0; bx--)
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

    /* Initialize the block index */
    btbl->blk_idx = hash_create(BLOCK_TBL_NENT);

    /* Insert every allocated block from rbtbl into the index */
    size_t nalloc = 0; //Number of allocated blocks found so far
    for(int bx = 0; bx < BLOCK_TBL_NENT; bx++)
    {
        blk_desc_t *blk = &(rbtbl->tbl[bx]);
        if(blk->bid >= 0) {
            hash_insert_item(btbl->blk_idx, (uint64_t)blk->local_addr, blk);
            nalloc++;
            if(nalloc == rbtbl->n_blocks) {
                break;
            }
        }
    }

    return true;
}

blk_desc_t *btbl_lookup(blk_tbl_t *btbl, void *target)
{
    return hash_get_item(btbl->blk_idx, (uint64_t)target);
}

bool btbl_free(blk_tbl_t *tbl, blk_desc_t *desc)
{
    hash_delete_item(tbl->blk_idx, (uint64_t)desc->local_addr);
    desc->bid = -(desc->bid);
    desc->local_addr = tbl->rbtbl->free;
    tbl->rbtbl->free = desc;
    tbl->rbtbl->n_blocks--;

    return true;
}

blk_desc_t *btbl_alloc(blk_tbl_t *tbl, void *local_addr)
{
    /* Pop a descriptor off the free list */
    blk_desc_t *desc = tbl->rbtbl->free;
    if(desc == NULL) {
        return NULL;
    } else {
        /* Remove from free list */
        desc->bid = -(desc->bid);
        tbl->rbtbl->free = (blk_desc_t*)desc->local_addr;

        /* Initialize and add to allocated list */
        tbl->rbtbl->n_blocks++;
        desc->local_addr = local_addr;
        hash_insert_item(tbl->blk_idx, (uint64_t)desc->local_addr, desc);

        /* Allocated blocks are considered modified */
        btbl_mark_mod(tbl, desc);

        return desc;
    }
}
