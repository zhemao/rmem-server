/* Super simple user-level allocator for rvm that just allocates whole blocks
 * at a time. This is the same as just calling rvm_blk_alloc(). Intended for
 * testing purposes only.
 */
#include "rvm.h"

void *simple_malloc(rvm_cfg_t *cfg, size_t size);

bool simple_free(rvm_cfg_t *cfg, void *buf);
