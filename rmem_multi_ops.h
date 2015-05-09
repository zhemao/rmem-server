#ifndef __RMEM_MULTI_OPS__
#define __RMEM_MULTI_OPS__

#include "rmem_table.h"

int rmem_multi_alloc(struct rmem_table *rmem, uint64_t *addrs, uint64_t size,
	uint32_t *tags, int n);
int rmem_multi_lookup(struct rmem_table *rmem, uint64_t *addrs,
	uint32_t *tags, int n);
int txn_multi_add_free(struct rmem_txn_list *list, uint64_t *addrs, int n);
int txn_multi_add_cp(struct rmem_txn_list *list, uint64_t *dsts, uint64_t *srcs,
	uint64_t *sizes, int n);

#endif
