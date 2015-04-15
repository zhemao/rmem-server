/* Implementation of user-facing functions for rvm */
#include "rvm.h"

rvm_cfg_t *rvm_cfg_create(rvm_opt_t opts)
{
    errno = ENOSYS;
    return NULL;
}

bool rvm_cfg_destroy(rvm_cfg_t cfg)
{
    errno = ENOSYS;
    return false;
}

rvm_txid_t rvm_txn_begin(rvm_cfg_t cfg)
{
    errno = ENOSYS;
    return -1;
}

bool rvm_txn_commit(rvm_cfg_t cfg, rvm_txid_t txid)
{
    errno = ENOSYS;
    return false;
}

void *rvm_alloc(rvm_cfg_t cfg, size_t size)
{
    errno = ENOSYS;
    return NULL;
}

bool rvm_free(rvm_cfg_t cfg, void *buf)
{
    errno = ENOSYS;
    return false;
}
