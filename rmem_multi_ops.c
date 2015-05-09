#include "rmem_multi_ops.h"

int rmem_multi_alloc(struct rmem_table *rmem, uint64_t *addrs, uint64_t *sizes,
	uint32_t *tags, int n)
{
    for (int i = 0; i < n; i++) {
	void *ptr = rmem_table_alloc(rmem, sizes[i], tags[i]);
	if (ptr == NULL)
	    return 1;
	addrs[i] = (intptr_t) ptr;
    }
    return 0;
}

int rmem_multi_lookup(struct rmem_table *rmem,
	uint64_t *addrs, uint32_t *tags, int n)
{
    for (int i = 0; i < n; i++) {
	void *ptr = rmem_table_lookup(rmem, tags[i]);
	if (ptr == NULL)
	    return 1;
	addrs[i] = (intptr_t) ptr;
    }
    return 0;
}

int txn_multi_add_free(struct rmem_txn_list *list, uint64_t *addrs, int n)
{
    int err;
    for (int i = 0; i < n; i++) {
	err = txn_list_add_free(list, (void *) addrs[i]);
	if (err != 0)
	    return err;
    }
    return 0;
}

int txn_multi_add_cp(struct rmem_txn_list *list, uint64_t *dsts, uint64_t *srcs,
	uint64_t *sizes, int n)
{
    int err;
    for (int i = 0; i < n; i++) {
	err = txn_list_add_cp(list,
		(void *) dsts[i], (void *) srcs[i], sizes[i]);
	if (err != 0)
	    return err;
    }
    return 0;
}
