#include "rmem_alloc.h"
#include "common.h"

#include <stdio.h>
#include <sys/mman.h>

static inline struct alloc_entry *entry_of_list(struct list_head *list)
{
	struct alloc_entry dummy;
	int offset = ((void *) &dummy.list) - ((void *) &dummy);
	return (struct alloc_entry *) (((void *) list) - offset);
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
	list_init(&rmem->list);
}

void free_rmem_table(struct rmem_table *rmem)
{
	struct list_head *node = rmem->list.next;
	struct alloc_entry *entry;

	while (node != &rmem->list) {
		struct list_head *next = node->next;
		entry = entry_of_list(node);
		free(entry->mem);
		list_delete(node);
		free(entry);
		node = next;
	}
}

void *rmem_alloc(struct rmem_table *rmem, size_t size, tag_t tag)
{
	int req_size = size + sizeof(struct alloc_entry *);
	struct alloc_entry *entry = NULL;

	TEST_Z(entry = malloc(sizeof(struct alloc_entry)));
	TEST_Z(entry->mem = malloc(req_size));
	entry->size = size;
	entry->tag = tag;
	list_append(&rmem->list, &entry->list);

	memcpy(entry->mem, &entry, sizeof(struct alloc_entry *));

	return entry->mem + sizeof(struct alloc_entry *);
}

void rmem_free(struct rmem_table *rmem, void *ptr)
{
	struct alloc_entry *entry = NULL;
	void *start = ptr - sizeof(struct alloc_entry *);

	memcpy(&entry, start, sizeof(struct alloc_entry *));

	list_delete(&entry->list);
	free(entry->mem);
	free(entry);
}

void dump_rmem_table(struct rmem_table *rmem)
{
	struct list_head *iter_node = rmem->list.next;
	struct alloc_entry *iter_entry;

	while (iter_node != &rmem->list) {
		iter_entry = entry_of_list(iter_node);
		printf("tag %u allocated %lu bytes at %p\n",
				iter_entry->tag, iter_entry->size,
				iter_entry->mem + sizeof(struct alloc_entry *));
		iter_node = iter_node->next;
	}
}
