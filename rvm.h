#ifndef _RVM_H_
#define _RVM_H_

/* This file includes the user-facing interface for recoverable virtual memory
 * NOTE: None of these functions are thread-safe
 *
 * UC Berkeley CS267 Sprint '15
 * Howard Mao, Nathan Pemberton, Joao Carreira
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "backends/rmem_generic_interface.h"

/** Transaction ID
 *  Valid txid's have positive values. Negative valued txid's indicate errors*/
typedef int8_t rvm_txid_t;

/** RVM configuration information.
 *  Use this to identify an instance of rvm. */
typedef struct rvm_cfg rvm_cfg_t;

/** The standard malloc function type. A function of this type can be passed
 * to rvm_cfg_create to provide a custom allocator. See rvm_alloc() for a
 * description of this function's intended behavior. */
typedef void *(*rvm_alloc_t)(rvm_cfg_t*, size_t);

/** The standard free function type. A function of this type can be passed
 * to rvm_cfg_create to provide a custom allocator. See rvm_free() for a
 * description of this function's intended behavior. */
typedef bool (*rvm_free_t)(rvm_cfg_t*, void*);

/** Options for an rvm configuration */
typedef struct
{
    char *host; /**< Host to connect to. */
    char *port; /**< Port to use for connection */
    bool recovery; /**< Are we recovering from a fault? */
    rvm_alloc_t alloc_fp; /**< Custom allocation function */
    rvm_free_t free_fp;   /**< Custom free for alloc_fp */
} rvm_opt_t;

/** Configure rvm.
 *  Will initialize the rvm system and enable the allocation of
 *  recoverable memory and the use of transactions. Free the rvm configuration
 *  using rvm_cfg_destroy(). If the recovery flag is set in rvm_opt_t then
 *  rvm_cfg_create will remap all previously allocated memory from the server.
 *  The user can use rvm_rec() to recover the structure of this memory.
 *
 *  \param[in] opts Options used to configure rvm. See rvm_opt_t for a
 *  description of each option.
 *  \returns A newly allocated configuration or NULL on error. Sets errno.
 */
rvm_cfg_t *rvm_cfg_create(rvm_opt_t *opts, create_rmem_layer_f create_rmem);

/** Free a previously created rvm configuration.
 *
 *  \param[in] cfg Configuration to destroy
 *  \returns true on success, false otherwise (sets errno)
 */
bool rvm_cfg_destroy(rvm_cfg_t *cfg);

/** Returns the block size used by rvm.
 * The block size is the size used by rvm_blk_alloc().
 */
size_t rvm_get_blk_sz(rvm_cfg_t *cfg);

/** Begin a transaction.
 *  rvm_begin_txn starts a new recoverable memory transaction. Any modifications
 *  to any recoverable memory region will be made atomically with respect to
 *  process failure. rvm_commit() will finalize any changes made.
 *
 *  \pre rvm must be configured (by calling rvm_configure()).
 *  \pre No other transaction may be active (no nested txns)
 *  \param[in] cfg Configuration to use for rvm
 *  \returns A transaction ID on success. -1 on error (sets errno).
 */
rvm_txid_t rvm_txn_begin(rvm_cfg_t* cfg);

/** Finalize a transaction.
 * After calling rvm_commit() any changes to a recoverable memory region since
 * the call to rvm_begin_txn associated with txid will be persisted atomically
 * to remote memory. rvm_commit() provides only eventual durability guarantees,
 * a return from rvm_commit() does not necessarily ensure durability.
 *
 * \pre Exactly one transaction must have been started (rvm_txn_begin) and not
 *  already committed.
 * \param[in] cfg Configuration to use for rvm
 * \param[in] txid The transaction id of the currently running transaction
 * \returns true for success. false otherwise, sets errno for specific error.
 */
bool rvm_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid);

/** Check transaction success (for debugging)
 * Check that all data now lives in the right places in the buddy node
 *
 * \pre Exactly one transaction must have been started (rvm_txn_begin) and not
 *  already committed.
 * \param[in] cfg Configuration to use for rvm
 * \param[in] txid The transaction id of the currently running transaction
 * \returns true for success. false otherwise, sets errno for specific error.
 */
bool check_txn_commit(rvm_cfg_t* cfg, rvm_txid_t txid);

/** Set allocator private information, will be preserved between failures */
bool rvm_set_alloc_data(rvm_cfg_t *cfg, void *alloc_data);

/** Retrieve allocator private information */
void *rvm_get_alloc_data(rvm_cfg_t *cfg);

/** Allocate a region of recoverable memory of at least size bytes.
 *
 * Any changes to memory returned by rvm_alloc() will be included in the
 * current transaction. Modification of this memory when not within an rvm
 * transaction results in undefined behavior. Memory must be freed using
 * rvm_free();.
 *
 * \pre rvm must be configured (by calling rvm_configure())
 * \param[in] cfg Configuration to use for rvm.
 * \param[in] size The size (in bytes) of the memory to allocate.
 * \returns A pointer to the beginning of recoverable memory
 */
void *rvm_alloc(rvm_cfg_t *cfg, size_t size);

/** Allocate a number of recoverable blocks of at least size "size" bytes.
 *
 * Any changes to memory returned by rvm_alloc() will be included in the
 * current transaction. Modification of this memory when not within an rvm
 * transaction results in undefined behavior. Memory must be freed using
 * rvm_free();. This function allocates an integer number of blocks.
 *
 * \pre rvm must be configured (by calling rvm_configure())
 * \param[in] cfg Configuration to use for rvm.
 * \param[in] size The size (in bytes) of the memory to allocate.
 * \returns A pointer to the beginning of recoverable memory
 */
void *rvm_blk_alloc(rvm_cfg_t* cfg, size_t size);

/** Free memory allocated by rvm_alloc().
 *
 * \param[in] cfg Configuration used to allocate buf
 * \param[in] buf Buffer to free
 * \returns true on success, false on error (sets errno)
 */
bool rvm_free(rvm_cfg_t* cfg, void *buf);

/** Free memory allocated by rvm_blk_alloc().
 *
 * \param[in] cfg Configuration used to allocate buf
 * \param[in] buf Buffer to free
 * \returns true on success, false on error (sets errno)
 */
bool rvm_blk_free(rvm_cfg_t* cfg, void *buf);

/** Recover the structure of recoverable memory.
 *  rvm_rec returns the address of the first recoverable allocation. Subsequent
 *  calls to rvm_rec return the next memory region that was allocated. rvm_rec
 *  returns NULL when there are no more allocations to recover.
 *
 *  It is important to note that all recoverable memory has already been mapped
 *  back into process memory by rvm_cfg_create. It is not necessary to call
 *  rvm_rec in order to read allocations. A typical usage would be to make the
 *  first allocation a structure that describes your recoverable data
 *  structures and then call rvm_rec only once to get this structure.
 *
 *  XXX Right now pointers do not work in rvm, as a result rvm_rec is the only
 *  way access recoverable data structures. I.E. the "typical usage" described
 *  above won't actually work.
 *
 *  XXX rvm_rec is quickly being deprecated. It's behavior could get a little
 *  skrewy (although if you don't free anything and use the malloc_simple
 *  allocator it should still work). No promises going forward.
 *
 *  \returns The first invocation of rvm_rec returns
 */
void *rvm_rec(rvm_cfg_t *cfg);

/** Set the user's private data. Pointer "data" will be available after
 *  recovery.
 *
 *  \pre Must be called within an active transaction.
 *
 *  \param[in] cfg RVM config
 *  \param[in] data Pointer to recoverable memory to persist
 *  \returns True on success, false otherwise (sets errno)
 */
bool rvm_set_usr_data(rvm_cfg_t *cfg, void *data);

/** Retrieve the user's private data.
 * Returns a pointer to recoverable memory that was set using rvm_set_usr_data()
 * before or after recovery. This is the primary method to reconstruct state
 * after a failure. Note that rvm_get_usr_data doesn't fetch remote data, it
 * simply allows the user to get a known location.
 *
 * \param[in] cfg RVM configuration info
 * \returns Whatever pointer (if any) was set using rvm_set_usr_data()
 */
void *rvm_get_usr_data(rvm_cfg_t *cfg);

#endif // _RVM_H_
