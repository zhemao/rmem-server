#ifndef _BUDDY_MALLOC_H_
#define _BUDDY_MALLOC_H_

/* An allocator based on the buddy algorithm */
#include "rvm.h"

void *buddy_meminit(rvm_cfg_t *cfg);

/* An instance of type rvm_alloc_t */
void *buddy_malloc(rvm_cfg_t *cfg, size_t n_bytes);

/* An instance of type rvm_free_t */
bool buddy_free(rvm_cfg_t *cfg, void *buf);

void print_map(rvm_cfg_t *cfg);

void buddy_memuse(rvm_cfg_t *cfg);

#endif // _BUDDY_MALLOC_H_
