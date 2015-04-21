#ifndef RMEM_TABLE_H
#define RMEM_TABLE_H

#include <stdlib.h>
#include <stdint.h>

#define RMEM_SIZE (1 << 14)
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
};

struct rmem_cp_info {
    struct list_head list;
    void *src;
    void *dst;
    size_t size;
};

struct rmem_cp_info_list {
    struct list_head head;
};

void init_rmem_table(struct rmem_table *rmem);
void *rmem_alloc(struct rmem_table *rmem, size_t size, tag_t tag);
void rmem_table_free(struct rmem_table *rmem, void *ptr);
void *rmem_table_lookup(struct rmem_table *rmem, tag_t tag);
void free_rmem_table(struct rmem_table *rmem);
void dump_rmem_table(struct rmem_table *rmem);

void cp_info_list_init(struct rmem_cp_info_list *list);
void cp_info_list_clear(struct rmem_cp_info_list *list);
int cp_info_list_add(struct rmem_cp_info_list *list,
	void *dst, void *src, size_t size);
void multi_cp(struct rmem_cp_info_list *list);

#endif
