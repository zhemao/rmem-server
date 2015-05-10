/* Private (internal) interfaces, types and constants for RVM */
#include <stdbool.h>
#include "rvm.h"
#include "backends/rmem_generic_interface.h"
#include "block_table.h"

/** Top-level rvm configuration info */
struct rvm_cfg
{
    /* Generic Config Info */
    size_t blk_sz;               /**< Size of minimum rvm allocation */
    bool in_txn;                 /**< Are we currently in a transaction? */
    rmem_layer_t* rmem_layer;    /**< State info for low-level interface */
    
    /* Block Table */
    blk_tbl_t blk_tbl;          /**< Info about all blocks tracked by rvm */

    /* User-level allocator */
    rvm_alloc_t alloc_fp;        /**< Function pointer for allocation */
    rvm_free_t free_fp;          /**< Function pointer for freeing */
    void *alloc_data;            /**< Allocator private data */
};
