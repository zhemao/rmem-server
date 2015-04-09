#ifndef RMEM_TABLE_H
#define RMEM_TABLE_H

#include <stdlib.h>
#include <stdint.h>

#define RMEM_SIZE (1 << 14)
#define MIN_SIZE (2 * sizeof(void*))

typedef uint32_t tag_t;

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

struct alloc_entry {
	struct list_head list;
	struct list_head free_list;
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
};

void init_rmem_table(struct rmem_table *rmem);
void *rmem_alloc(struct rmem_table *rmem, size_t size, tag_t tag);
void rmem_free(struct rmem_table *rmem, void *ptr);
void *rmem_lookup(struct rmem_table *rmem, tag_t tag, size_t *size);
void free_rmem_table(struct rmem_table *rmem);
void dump_rmem_table(struct rmem_table *rmem);

#endif
