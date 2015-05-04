/* Private (internal) interfaces, types and constants for RVM */
#include "rvm.h"
#include "rmem_generic_interface.h"
#include <stdbool.h>

/* Block table always has this tag */
#define BLOCK_TBL_ID 0

/** Number of block descriptors in the block table */
#define BLOCK_TBL_SIZE \
    ((sysconf(_SC_PAGESIZE) - sizeof(blk_tbl_t)) / sizeof(block_desc_t))

/* Get the tag for a real block 
 int BX - index of the block in the block table */
#define BLK_REAL_TAG(BX) (BLOCK_TBL_ID + BX*2 + 1)

/* Get the tag for a shadow-block
   int BX - the index of the block in the block table */
#define BLK_SHDW_TAG(BX) (BLOCK_TBL_ID + BX*2 + 2)

/** Describes a block (these are the entries in the block table)
 * For now blocks are fixed-sized (page size) */
typedef struct
{
    uint32_t bid;        /**< Block identifier (tag) on the server */
    void *local_addr;    /**< Address of block on client (also flag for whether block is being used)*/
    void* blk_rec;       // ugly
} block_desc_t;

/** The block table is a page-sized list of every block tracked by rvm */
typedef struct
{
    //uint64_t raddr; /**< Remote address of block table */
    
    uint64_t n_blocks; /**< Counter of how many blocks are currently being used (also shadow blocks) */

    void *usr_data; /**< A user-defined pointer to recoverable data */
    void *alloc_data; /**< Pointer to custom allocator data */

    /** Table of block descriptors. It has BLOCK_TBL_SIZE entries. */
    block_desc_t tbl[];
} blk_tbl_t;

/** Top-level rvm configuration info */
struct rvm_cfg
{
    /* Generic Config Info */
    size_t blk_sz;               /**< Size of minimum rvm allocation */
    bool in_txn;                 /**< Are we currently in a transaction? */
    rmem_layer_t* rmem_layer;    /**< State info for low-level interface */
    
    /* Block Table */
    blk_tbl_t *blk_tbl;          /**< Info about all blocks tracked by rvm */
    void* blk_tbl_rec;           /**< Ugly hack to get IB registration info */

    /* User-level allocator */
    rvm_alloc_t alloc_fp;        /**< Function pointer for allocation */
    rvm_free_t free_fp;          /**< Function pointer for freeing */
    void *alloc_data;            /**< Allocator private data */
};
