/*
 * block_table.h

 *
 *  The block table is a data structure used to keep track of blocks that are
 *  managed by the rmem layer. The block table is not responsible for
 *  interacting with the rmem layer, it simply tracks block descriptors.
 *
 *  Created on: May 8, 2015
 *      Author: Nathan Pemberton
 */

#ifndef BLOCK_TABLE_H_
#define BLOCK_TABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"
#include "data/hash.h"

/** Describes a block (these are the entries in the block table)
 * For now blocks are fixed-sized (page size) */
typedef struct
{
    /** Block identifier. Use BLK_REAL_TAG and BLK_SHDW_TAG to convert to
     * a tag in the rmem layer. Do not modify outside of btbl_* functions!
     * negative values indicate the block is free. */
    int32_t bid;

    /** Address of block on client. If bid == BID_INVAL then local_addr points
     * to the next free block descriptor */
    void *local_addr;

    /* Data registration field used by rmem layer */
    void* blk_rec;
} blk_desc_t;

/** The block table is a page-sized list of every block tracked by rvm */
typedef struct
{
    uint64_t n_blocks; /**< Counter of how many blocks are currently being used (also shadow blocks) */

    void *usr_data; /**< A user-defined pointer to recoverable data */
    void *alloc_data; /**< Pointer to custom allocator data */

    /* Linked list of free block descriptors, local_addr field used as next
     * pointer */
    blk_desc_t *free;

    /** Table of block descriptors. It has BLOCK_TBL_SIZE entries. */
    blk_desc_t tbl[];
} raw_blk_tbl_t;

/* An in-memory index for a raw block table.
 * This structure is not intended to be preserved over rmem. */
typedef struct
{
    /* The raw block table being indexed */
    raw_blk_tbl_t *rbtbl;

    /* Bit array of changed entries in the block table */
    bitmap_t *blk_chlist;

    /* Index of every allocated block and it's local address.
     * key = local addr (void*), val = block descriptor (blk_desc_t*) */
    hash_t blk_idx;

} blk_tbl_t;

/* Block table always has this tag */
#define BLOCK_TBL_ID 0

/* An invalid block ID, used like NULL */
#define BID_INVAL UINT32_MAX

/** Number of block descriptors in the block table */
//#define BLOCK_TBL_NENT (1 << 16)
#define BLOCK_TBL_NENT (1 << 10)

/* Size (in bytes) of the block table struct
 * This is rounded up to the nearest multiple of 4096 */
#define BLOCK_TBL_SIZE ((((sizeof(blk_tbl_t) + sizeof(blk_desc_t)*BLOCK_TBL_NENT) / 4096) + 1) * 4096)

/* Number of pages needed to contain block table */
#define BLOCK_TBL_NPG (BLOCK_TBL_SIZE / 4096)

/* Get the tag for a real block
 int BX - index of the block in the block table */
#define BLK_REAL_TAG(BX) (BLOCK_TBL_NPG + BX*2 + 1)

/* Get the tag for a shadow-block
   int BX - the index of the block in the block table */
#define BLK_SHDW_TAG(BX) (BLOCK_TBL_NPG + BX*2 + 2)

/* Initialize a freshly allocated raw block table */
bool rbtbl_init(raw_blk_tbl_t *rbtbl);

/* Initialize a block table index from a raw block table */
bool btbl_init(blk_tbl_t *btbl, raw_blk_tbl_t *rbtbl);

/* Find an existing address in the block table
 * \param[in] tbl Table to look in
 * \param[in] target Address of memory region. Must be page-aligned.
 * \returns The block descriptor with target address or NULL if none found. */
blk_desc_t *btbl_lookup(blk_tbl_t *tbl, void *target);

/* Mark the block descriptor as free in the block table.
 * \param[in] tbl Table containing descriptor to be freed.
 * \param[in] desc Descriptor to free
 */
bool btbl_free(blk_tbl_t *tbl, blk_desc_t *desc);

/* Allocate a slot in the block table.
 * \param[in] tbl Table to insert into
 * \param[in] local_addr The desired local address for this block
 * \returns An unallocated block descriptor or NULL if the table is full
 */
blk_desc_t *btbl_alloc(blk_tbl_t *tbl, void *local_addr);

/* Mark a block as modified. */
static inline void btbl_mark_mod(blk_tbl_t *tbl, blk_desc_t *blk)
{
    if(blk->bid < 0)
        return;

    BITSET(tbl->blk_chlist, blk->bid);
}

static inline void btbl_clear_mod(blk_tbl_t *tbl, blk_desc_t *blk)
{
    if(blk->bid < 0)
        return;

    BITCLEAR(tbl->blk_chlist, blk->bid);
}

/* Test if a block has been modified
 * \returns True if block has been modified, false otherwise */
static inline bool btbl_test_mod(blk_tbl_t *tbl, blk_desc_t *blk)
{
    if(blk->bid < 0)
        return false;

    return BITTEST(tbl->blk_chlist, blk->bid);
}

#endif /* BLOCK_TABLE_H_ */
