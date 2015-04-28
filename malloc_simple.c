#include "rvm.h"

void *simple_malloc(rvm_cfg_t *cfg, size_t size)
{
    return rvm_blk_alloc(cfg, size);
}

bool simple_free(rvm_cfg_t *cfg, void *buf)
{
    return rvm_blk_free(cfg, buf);
}
