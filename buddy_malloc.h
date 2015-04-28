#ifndef _BUDDY_MALLOC_H_
#define _BUDDY_MALLOC_H_

/* An allocator based on the buddy algorithm */
#include "rvm.h"

void *buddy_meminit(rvm_cfg_t *cfg);

void *buddy_memalloc(rvm_cfg_t *cfg, size_t n_bytes);

void buddy_memfree(rvm_cfg_t *cfg, void *buf);

void print_map(rvm_cfg_t *cfg);

void buddy_memuse(rvm_cfg_t *cfg);

#endif _BUDDY_MALLOC_H_
