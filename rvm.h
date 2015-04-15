/* This file includes the user-facing interface for recoverable virtual memory
 * UC Berkeley CS267 Sprint '15
 * Howard Mao, Nathan Pemberton, Joao Carreira
 */
#include <stdint.h>
#include <stdbool.h>

/* Transaction ID
 * Valid txid's have positive values. Negative valued txid's indicate errors*/
typedef int8_t rvm_txid_t;

typedef struct
{
    //Not sure what to put here yet
} rvm_opt_t;

/* RVM configuration information. Use this to identify an instance of rvm. */
typedef struct rvm_cfg rvm_cfg_t;

/** Configure rvm.
 *  Will initialize the rvm system and enable the allocation of
 *  recoverable memory and the use of transactions. Free the rvm configuration
 *  using rvm_cfg_destroy().
 *
 *  \param[in] opts Options used to configure rvm. See rvm_opt_t for a
 *  description of each option.
 *  \returns A newly allocated configuration or NULL on error. Sets errno.
 */
rvm_cfg_t *rvm_cfg_create(rvm_opt_t opts);

/** Free a previously created rvm configuration.
 *
 *  \param[in] cfg Configuration to destroy
 *  \returns true on success, false otherwise (sets errno)
 */
bool rvm_cfg_destroy(rvm_cfg_t cfg);

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
rvm_txid_t rvm_txn_begin(rvm_cfg_t cfg);

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
bool rvm_txn_commit(rvm_cfg_t cfg, rvm_txid_t txid);

/** Allocate a region of recoverable memory of at least size "size" bytes.
 *
 * Any changes to memory returned by rvm_alloc() will be included in the
 * current transaction. Modification of this memory when not within an rvm
 * transaction results in undefined behavior. Memory must be freed using
 * rvm_free();
 *
 * \pre rvm must be configured (by calling rvm_configure())
 * \param[in] cfg Configuration to use for rvm.
 * \param[in] size The size (in bytes) of the memory to allocate.
 * \returns A pointer to the beginning of recoverable memory
 */
void *rvm_alloc(rvm_cfg_t cfg, size_t size);

/** Free memory allocated by rvm_alloc().
 *
 * \param[in] cfg Configuration used to allocate buf
 * \param[in] buf Buffer to free
 * \returns true on success, false on error (sets errno)
 */
bool rvm_free(rvm_cfg_t cfg, void *buf);
