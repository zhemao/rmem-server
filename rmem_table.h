#ifndef RMEM_TABLE_H
#define RMEM_TABLE_H

#include <stdlib.h>
#include <stdint.h>
#include "data/hash.h"

//#define RMEM_SIZE (1 << 15)
#define RMEM_SIZE (1 << 17)
#define MIN_SIZE (2 * sizeof(void*))
#define NUM_BUCKETS 1024

typedef uint32_t tag_t;

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

struct alloc_entry {
    struct list_head list;
    struct list_head free_list;
    struct list_head htable;
    void *start;
    size_t size;
    tag_t tag;
    char free;
};

struct rmem_table {
    void *mem;
    struct list_head list;
    struct list_head free_list;
    size_t alloc_size;
    struct list_head *htable;
    hash_t tag_to_addr;
};

enum rmem_txn_type {
    TXN_CP,
    TXN_FREE
};

struct rmem_txn {
    struct list_head list;
    enum rmem_txn_type type;
    void *src;
    void *dst;
    size_t size;
};

struct rmem_txn_list {
    struct list_head head;
};

void init_rmem_table(struct rmem_table *rmem);
void *rmem_table_alloc(struct rmem_table *rmem, size_t size, tag_t tag);
void rmem_table_free(struct rmem_table *rmem, void *ptr);
void *rmem_table_lookup(struct rmem_table *rmem, tag_t tag);
void free_rmem_table(struct rmem_table *rmem);
void dump_rmem_table(struct rmem_table *rmem);

void txn_list_init(struct rmem_txn_list *list);
void txn_list_clear(struct rmem_txn_list *list);
int txn_list_add_cp(struct rmem_txn_list *list,
	void *dst, void *src, size_t size);
//int txn_list_add_alloc(struct rmem_txn_list *list, size_t size);
int txn_list_add_free(struct rmem_txn_list *list, void *addr);
void txn_commit(struct rmem_table *rmem, struct rmem_txn_list *list);

#endif
