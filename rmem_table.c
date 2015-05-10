#include "rmem_table.h"
#include "common.h"

#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>
#include "utils/log.h"

#define HASH_SIZE 10000
#define DATA_OFFSET (sizeof(struct alloc_entry *))

static inline struct alloc_entry *entry_of_list(struct list_head *list)
{
    struct alloc_entry dummy;
    int offset = ((uintptr_t) &dummy.list) - ((uintptr_t) &dummy);
    return (struct alloc_entry *) (((void *) list) - offset);
}

static inline struct alloc_entry *entry_of_free_list(struct list_head *list)
{
    struct alloc_entry dummy;
    int offset = ((uintptr_t) &dummy.free_list) - ((uintptr_t) &dummy);
    return (struct alloc_entry *) (((void *) list) - offset);
}

static inline struct alloc_entry *entry_of_htable(struct list_head *list)
{
    struct alloc_entry dummy;
    int offset = ((uintptr_t) &dummy.htable) - ((uintptr_t)&dummy);
    return (struct alloc_entry *) (((void *) list) - offset);
}

static inline struct rmem_txn *txn_of_list(struct list_head *list)
{
    struct rmem_txn txn;
    int offset = ((uintptr_t) &txn.list) - ((uintptr_t) &txn);
    return (struct rmem_txn *) (((void *) list) - offset);
}

static inline void list_init(struct list_head *node)
{
    node->prev = node;
    node->next = node;
}

static inline void list_insert(struct list_head *prev, struct list_head *node)
{
    node->prev = prev;
    node->next = prev->next;
    prev->next = node;
    node->next->prev = node;
}

static inline void list_append(struct list_head *start, struct list_head *node)
{
    list_insert(start->prev, node);
}

static inline void list_delete(struct list_head *node)
{
    struct list_head *prev = node->prev;
    struct list_head *next = node->next;

    prev->next = next;
    next->prev = prev;
}

static inline int list_empty(struct list_head *list)
{
    return (list->next == list);
}

void init_rmem_table(struct rmem_table *rmem)
{
    int i;

    rmem->mem = mmap(NULL, RMEM_SIZE, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (rmem->mem == MAP_FAILED) {
        fprintf(stderr, "Failed to map remote memory\n");
        exit(EXIT_FAILURE);
    }
    list_init(&rmem->list);
    list_init(&rmem->free_list);
    rmem->alloc_size = 0;

    rmem->htable = (struct list_head*)malloc(sizeof(struct list_head) * NUM_BUCKETS);
    if (rmem->htable == NULL) {
	fprintf(stderr, "Failed to allocate hash table\n");
	exit(EXIT_FAILURE);
    }

    for (i = 0; i < NUM_BUCKETS; i++)
	list_init(&rmem->htable[i]);

    rmem->nblocks = 0;
}

void free_rmem_table(struct rmem_table *rmem)
{
    struct list_head *node = rmem->list.next;
    struct alloc_entry *entry;

    munmap(rmem->mem, RMEM_SIZE);

    while (node != &rmem->list) {
        struct list_head *next = node->next;
        entry = entry_of_list(node);
        list_delete(node);
        free(entry);
        node = next;
    }

    free(rmem->htable);
}

static inline struct list_head *free_list_to_list(struct rmem_table *rmem,
        struct list_head *fl_node)
{
    struct alloc_entry *iter_entry;
    if (fl_node == &rmem->free_list)
        return &rmem->list;
    iter_entry = entry_of_free_list(fl_node);
    return &iter_entry->list;
}

static inline void set_free_ptrs(struct rmem_table *rmem,
        struct list_head *free_node)
{
    struct list_head *last_free = free_node->prev;
    struct list_head *next_free = free_node->next;
    struct list_head *start_node, *iter_node;
    struct alloc_entry *iter_entry;

    last_free = free_list_to_list(rmem, last_free);
    next_free = free_list_to_list(rmem, next_free);
    start_node = free_list_to_list(rmem, free_node);

    iter_node = start_node->prev;
    while (iter_node != last_free) {
        iter_entry = entry_of_list(iter_node);
        iter_entry->free_list.next = free_node;
        iter_node = iter_node->prev;
    }

    iter_node = start_node->next;
    while (iter_node != next_free) {
        iter_entry = entry_of_list(iter_node);
        iter_entry->free_list.prev = free_node;
        iter_node = iter_node->next;
    }
}

static inline void reserve_entry(struct rmem_table *rmem,
        struct alloc_entry *entry, size_t req_size)
{
    size_t free_size = entry->size - req_size;
    struct alloc_entry *free_entry;
    void *entry_end = entry->start + req_size, *free_entry_end;
    struct list_head *last_free = entry->free_list.prev;
    struct list_head *next_free = entry->free_list.next;
    struct list_head *free_node;

    entry->size = req_size;
    entry->free = 0;
    list_delete(&entry->free_list);

    if (free_size < MIN_SIZE || entry->list.next == &rmem->list) {
        free_node = next_free;
    } else if (entry->list.next == entry->free_list.next) {
        free_entry = entry_of_free_list(entry->free_list.next);
        free_entry_end = free_entry->start + free_entry->size;
        free_entry->start = entry_end;
        free_entry->size = (uintptr_t)free_entry_end - (uintptr_t)entry_end;
        free_node = &free_entry->free_list;
    } else {
        TEST_Z(free_entry = (struct alloc_entry*)malloc(sizeof(struct alloc_entry)));
        free_entry->free = 1;
        free_entry->tag = 0;
        free_entry->size = free_size;
        free_entry->start = entry->start + req_size;
        list_insert(&entry->list, &free_entry->list);
        list_insert(last_free, &free_entry->free_list);
        free_node = &free_entry->free_list;
    }

    set_free_ptrs(rmem, free_node);
}

static struct alloc_entry *find_entry(struct rmem_table *rmem, tag_t tag)
{
    int bucket;
    struct list_head *list, *node;
    struct alloc_entry *entry;

    bucket = tag % NUM_BUCKETS;
    list = &rmem->htable[bucket];
    node = list->next;

    while (node != list) {
	entry = entry_of_htable(node);
	if (entry->tag == tag)
	    return entry;
	node = node->next;
    }

    return NULL;
}

void *rmem_table_alloc(struct rmem_table *rmem, size_t size, tag_t tag)
{
    size_t req_size = size + DATA_OFFSET;
    struct alloc_entry *entry = NULL;
    struct list_head *free_node;
    int bucket;

    /* Warn if the tag is not unique, then return the original allocation as
       if the caller had used rmem_table_lookup. This can happen if the user
       fails before committing any allocations, and doesn't know it already
       allocated something. */
    entry = find_entry(rmem, tag);
    if (entry != NULL) {
        LOG(5, ("Requested tag %d is not unique\n", tag));
    	return entry->start + DATA_OFFSET;
    }

    free_node = rmem->free_list.next;

    // next_free == start node means we've reached the end
    while (free_node != &rmem->free_list) {
        entry = entry_of_free_list(free_node);
        // make sure entry is actually free
        TEST_Z(entry->free);
        if (entry->size >= req_size)
            break;
        free_node = free_node->next;
    }

    if (free_node == &rmem->free_list) {
        // make sure we haven't run out of memory
        if (rmem->alloc_size + req_size > RMEM_SIZE) {
            LOG(5, ("Out of memory\n"));
            return NULL;
        }

        TEST_Z(entry = (struct alloc_entry*)malloc(sizeof(struct alloc_entry)));
        list_append(&rmem->list, &entry->list);
        entry->free_list.next = &rmem->free_list;
        entry->free_list.prev = free_node->prev;
        entry->start = rmem->mem + rmem->alloc_size;
        rmem->alloc_size += req_size;
        entry->size = req_size;
        entry->free = 0;
        entry->tag = tag;
    } else {
        entry->tag = tag;
        reserve_entry(rmem, entry, req_size);
    }

    rmem->nblocks++;
    bucket = tag % NUM_BUCKETS;
    list_append(&rmem->htable[bucket], &entry->htable);

    memcpy(entry->start, &entry, sizeof(struct alloc_entry *));

    return entry->start + DATA_OFFSET;
}

static inline struct alloc_entry *merge_free_blocks(
        struct rmem_table *rmem, struct alloc_entry *entry)
{
    struct alloc_entry *prev_entry, *next_entry;
    void *start, *end;

    start = entry->start;
    end = entry->start + entry->size;

    if (entry->list.prev != &rmem->list) {
        prev_entry = entry_of_list(entry->list.prev);
        if (prev_entry->free) {
            start = prev_entry->start;
            prev_entry->size = (uintptr_t)end - (uintptr_t)start;
            list_delete(&entry->list);
	    list_delete(&entry->htable);
            free(entry);
            entry = prev_entry;
        } else {
            struct list_head *last_free;
            start = prev_entry->start + prev_entry->size;
            entry->start = start;
            entry->size = (uintptr_t)end - (uintptr_t)start;
            last_free = entry->free_list.prev;
            list_insert(last_free, &entry->free_list);
        }
    } else {
        list_append(&rmem->free_list, &entry->free_list);
    }

    if (entry->list.next == &rmem->list) {
        list_delete(&entry->list);
        list_delete(&entry->free_list);
	list_delete(&entry->htable);
        rmem->alloc_size -= entry->size;
        free(entry);
        return NULL;
    }

    next_entry = entry_of_list(entry->list.next);
    if (next_entry->free) {
        end = next_entry->start + next_entry->size;
        list_delete(&next_entry->list);
        list_delete(&next_entry->free_list);
	list_delete(&next_entry->htable);
        free(next_entry);
    } else {
        end = next_entry->start;
    }
    entry->size = (uintptr_t)end - (uintptr_t)start;

    return entry;
}

static inline void dump_free_list(struct rmem_table *rmem)
{
    struct list_head *iter_node = rmem->list.next;
    struct alloc_entry *iter_entry;

    printf("%p <- %p -> %p\n",
            rmem->free_list.prev,
            &rmem->free_list,
            rmem->free_list.next);

    while (iter_node != &rmem->list) {
        iter_entry = entry_of_list(iter_node);
        printf("%p <- %p -> %p\n",
                iter_entry->free_list.prev,
                &iter_entry->free_list,
                iter_entry->free_list.next);
        iter_node = iter_node->next;
    }
    printf("-----------\n");
}

void rmem_table_free(struct rmem_table *rmem, void *ptr)
{
    void *start = ptr - DATA_OFFSET;
    struct alloc_entry *entry;

    memcpy(&entry, start, sizeof(struct alloc_entry *));

    if (entry->free) {
	LOG(1, ("Block at %p has already been freed.\n", ptr));
	return;
    }

    rmem->nblocks--;

    entry->free = 1;
    entry->tag = 0;
    //dump_free_list(rmem);
    entry = merge_free_blocks(rmem, entry);
    //dump_free_list(rmem);
    if (entry != NULL)
        set_free_ptrs(rmem, &entry->free_list);
    //dump_free_list(rmem);
}

void *rmem_table_lookup(struct rmem_table *rmem, tag_t tag)
{
    struct alloc_entry *entry;

    entry = find_entry(rmem, tag);

    if (entry == NULL)
	return NULL;

    return entry->start + DATA_OFFSET;
}

void dump_rmem_table(struct rmem_table *rmem)
{
    struct list_head *iter_node = rmem->list.next;
    struct alloc_entry *iter_entry;

    while (iter_node != &rmem->list) {
        iter_entry = entry_of_list(iter_node);
        if (iter_entry->free)
            printf("%p - size %lu (free)\n",
                iter_entry->start, iter_entry->size);
        else
            printf("%p - size %lu, tag %d\n",
                iter_entry->start, iter_entry->size,
                iter_entry->tag);
        iter_node = iter_node->next;
    }
    //dump_free_list(rmem);
}

void txn_list_init(struct rmem_txn_list *list)
{
    list_init(&list->head);
}

void txn_list_clear(struct rmem_txn_list *list)
{
    struct rmem_txn *txn;

    while (!list_empty(&list->head)) {
	txn = txn_of_list(list->head.next);
	list_delete(list->head.next);
	free(txn);
    }
}

int txn_list_add_cp(struct rmem_txn_list *list,
	void *dst, void *src, size_t size)
{
    struct rmem_txn *txn;

    txn = (struct rmem_txn*)malloc(sizeof(struct rmem_txn));
    if (txn == NULL)
	return -1;

    txn->type = TXN_CP;
    txn->src = src;
    txn->dst = dst;
    txn->size = size;

    list_append(&list->head, &txn->list);

    return 0;
}

/*int txn_list_add_alloc(struct rmem_txn_list *list, size_t size)
{
    struct rmem_txn *txn;

    txn = malloc(sizeof(struct rmem_txn));
    if (txn == NULL)
	return -1;

    txn->type = TXN_ALLOC;
    txn->src = 0;
    txn->dst = 0;
    txn->size = size;

    list_append(&list->head, &txn->list);

    return 0;
}*/

int txn_list_add_free(struct rmem_txn_list *list, void *addr)
{
    struct rmem_txn *txn;

    txn = (struct rmem_txn*)malloc(sizeof(struct rmem_txn));
    if (txn == NULL)
	return -1;

    txn->type = TXN_FREE;
    txn->src = addr;
    txn->dst = 0;
    txn->size = 0;

    list_append(&list->head, &txn->list);

    return 0;
}

void txn_commit(struct rmem_table *rmem, struct rmem_txn_list *list)
{
    struct rmem_txn *txn;
    struct list_head *node = list->head.next;

    while (node != &list->head) {
        txn = txn_of_list(node);
	switch (txn->type) {
	case TXN_CP:
	    memcpy(txn->dst, txn->src, txn->size);
	    break;
	case TXN_FREE:
	    rmem_table_free(rmem, txn->src);
	    break;
	default:
	    fprintf(stderr, "Unknown TXN type\n");
	    abort();
	}
        node = node->next;
    }
}

void init_rmem_iterator(struct rmem_iterator *iter, struct rmem_table *rmem)
{
    iter->rmem = rmem;
    iter->cur_node = rmem->list.next;
    iter->block_ind = 0;
}

int rmem_iter_next_set(struct rmem_iterator *iter, tag_addr_entry_t *tag_addr)
{
    int entries_left = iter->rmem->nblocks - iter->block_ind;
    int nentries = MIN(entries_left, TAG_ADDR_MAP_SIZE_MSG);
    int ind = 0;
    struct alloc_entry *entry;

    while (ind < nentries) {
	assert(iter->cur_node != &iter->rmem->list);

	entry = entry_of_list(iter->cur_node);
	iter->cur_node = iter->cur_node->next;

	if (entry->free)
	    continue;

	tag_addr[ind].tag = entry->tag;
	tag_addr[ind].addr = (uintptr_t) (entry->start + DATA_OFFSET);
	ind++;
    }

    iter->block_ind += nentries;

    return nentries;
}

int rmem_iter_finished(struct rmem_iterator *iter)
{
    return iter->block_ind == iter->rmem->nblocks;
}
