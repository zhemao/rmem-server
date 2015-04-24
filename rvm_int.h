/* Private (internal) interfaces, types and constants for RVM */
#include "rvm.h"
#include "rmem.h"
#include <stdbool.h>

/* Block table always has this tag */
#define BLOCK_TBL_ID 0

/** Number of block descriptors in the block table */
#define BLOCK_TBL_SIZE \
    ((sysconf(_SC_PAGESIZE) - sizeof(blk_tbl_t)) / sizeof(block_desc_t))



/** Describes a block (these are the entries in the block table)
 * For now blocks are fixed-sized (page size) */
typedef struct
{
    uint32_t bid;        /**< Block identifier (tag) on the server */
    void *local_addr;    /**< Address of block on client (also flag for whether block is being used)*/
    uint64_t raddr;      /**< Address of block on server */
    struct ibv_mr *mr;   /**< IB registration info (invalid during rec) */

    int size;            /**< size of this block (no longer stuck with page size) */
} block_desc_t;

/** The block table is a page-sized list of every block tracked by rvm */
typedef struct
{
    uint64_t raddr; /**< Remote address of block table */
    
    uint64_t n_blocks; /**< Counter of how many blocks are currently being used (also shadow blocks) */

    /** Table of block descriptors. It has BLOCK_TBL_SIZE entries. */
    block_desc_t tbl[];
} blk_tbl_t;

/** Top-level rvm configuration info */
struct rvm_cfg
{
    struct rmem rmem;            /**< rmem connection information */
    size_t blk_sz;               /**< Size of minimum rvm allocation */
    blk_tbl_t *blk_tbl;          /**< Info about all blocks tracked by rvm */
    struct ibv_mr *blk_tbl_mr;   /**< IB registration info for block table */
    bool in_txn;                 /**< Are we currently in a transaction? */
};
