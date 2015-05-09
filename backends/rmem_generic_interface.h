#ifndef _BACKUP_INTERFACE_H_
#define _BACKUP_INTERFACE_H_

#include <stdint.h>

typedef struct rmem_layer rmem_layer_t;

/* Connect to the RMEM backend.
 *
 * Args:
 * \param[in] rcfg rmem layer config info
 * \param[in] host Hostname
 * \param[in] port Port number
 */
typedef void (*rmem_connect_f)(rmem_layer_t* rcfg, char* host, char* port);

/* Disconnect from RMEM backend
 *
 * \param[in] rcfg Remote layer configuration info
 */
typedef void (*rmem_disconnect_f)(rmem_layer_t* rcfg);

/* Allocate a block of memory on the rmem layer
 * \param[in] rcfg RMEM layer config info
 * \param[in] size Requested block size
 * \param[in] tag Requested tag to identify the block
 * \returns A 64-bit identifier for this memory on the rmem layer
 */
typedef uint64_t (*rmem_malloc_f)(rmem_layer_t* rcfg,
        size_t size, uint32_t tag);

/* Free a block of memory returned by rmem_malloc
 * \param[in] rcfg RMEM layer config info
 * \param[in] tag Tag of memory to free
 */
typedef int (*rmem_free_f)(rmem_layer_t* rcfg, uint32_t tag);

/* Copy a block of memory to the rmem layer.
 * \param[in] rcfg RMEM layer config info
 * \param[in] tag Tag of destination
 * \param[in] src Local copy of data
 * \param[in] src_reg Registration info for the source
 * \param[in] size size of the region to copy
 *
 * \returns 0 on success, errno otherwise
 *  */
typedef int (*rmem_put_f)(rmem_layer_t* rcfg, uint32_t tag,
        void *src, void *src_reg, size_t size);

/* Fetch a block from the rmem layer.
 * \param[in] rcfg RMEM layer config info
 * \param[in] dest Local buffer to copy data into
 * \param[in] dest_mr Registration info for dest
 * \param[in] tag Tag for remote block
 * \param[in] size Size of block to copy
 *
 * \returns 0 on success, errno otherwise
 */
typedef int (*rmem_get_f)(rmem_layer_t* rcfg, void *dest,
        void *dest_mr, uint32_t tag, size_t size);

/* Atomically copy a set of blocks in the rmem layer
 * \param[in] rcfg RMEM layer config info
 * \param[in] tags_src Array of source tags
 * \param[in] tags_dst Array of destination tags
 * \param[in] tags_sz  Array of sizes of blocks to copy
 * \param[in] nblk     Number of blocks to be copies (size of arrays)
 *
 * \returns 0 on success, Non-0 on failure
 */
typedef int (*rmem_atomic_commit_f)(rmem_layer_t* rcfg,
        uint32_t* tags_src, uint32_t* tags_dst, uint32_t* sizes, uint32_t ntag);

/* Register a local block with the rmem layer
 * \param[in] rcfg RMEM layer config info
 * \param[in] buf  Buffer to register
 * \param[in] size Size of buffer to register
 */
typedef void* (*rmem_register_data_f)(rmem_layer_t* rcfg,
        void* buf, size_t size);

/* Deregister a local block of data with the rmem layer
 * \param[in] rcfg RMEM layer config info
 * \param[in] buf Buffer to deregister
 */
typedef void (*rmem_deregister_data_f)(rmem_layer_t* rcfg, void* buf);

/* Allocate n memory regions and write their addresses to addrs
 * \param[in] rcfg RMEM layer config info
 * \param[in] addrs pointer to where the results should be placed
 * \param[in] sizes sizes of allocated regions
 * \param[in] tags tags of allocated regions
 * \param[in] n number of regions to allocate
 * \returns 0 on success
 */
typedef int (*rmem_multi_malloc_f)(rmem_layer_t *rcfg,
	uint64_t *addrs, uint64_t *sizes, uint32_t *tags, uint32_t n);

typedef int (*rmem_multi_free_f)(rmem_layer_t *rcfg,
	uint32_t *tags, uint32_t n);

/* Set up the rmem layer
 * \returns A struct containing configuration info for the rmem backend.
 */
typedef rmem_layer_t* (*create_rmem_layer_f)();

typedef struct data_record {
} data_record_t;

typedef struct rmem_layer {
    rmem_connect_f connect;
    rmem_disconnect_f disconnect;
    rmem_malloc_f malloc;
    rmem_put_f put;
    rmem_get_f get;
    rmem_free_f free;
    rmem_atomic_commit_f atomic_commit;
    rmem_register_data_f register_data;
    rmem_deregister_data_f deregister_data;
    rmem_multi_malloc_f multi_malloc;
    rmem_multi_free_f multi_free;

    void* layer_data; // layer-specific data
} rmem_layer_t;

#endif // _BACKUP_INTERFACE_H_

