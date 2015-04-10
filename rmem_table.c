#include "rmem_table.h"
#include "common.h"

#include <stdio.h>
#include <sys/mman.h>

#define DATA_OFFSET (sizeof(struct alloc_entry *))

static inline struct alloc_entry *entry_of_list(struct list_head *list)
{
	struct alloc_entry dummy;
	int offset = ((void *) &dummy.list) - ((void *) &dummy);
	return (struct alloc_entry *) (((void *) list) - offset);
}

static inline struct alloc_entry *entry_of_free_list(struct list_head *list)
{
	struct alloc_entry dummy;
	int offset = ((void *) &dummy.free_list) - ((void *) &dummy);
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
	rmem->mem = mmap(NULL, RMEM_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (rmem->mem == MAP_FAILED) {
		fprintf(stderr, "Failed to map remote memory\n");
		exit(EXIT_FAILURE);
	}
	list_init(&rmem->list);
	list_init(&rmem->free_list);
	rmem->alloc_size = 0;
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
		free_entry->size = free_entry_end - entry_end;
		free_node = &free_entry->free_list;
	} else {
		TEST_Z(free_entry = malloc(sizeof(struct alloc_entry)));
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

void *rmem_alloc(struct rmem_table *rmem, size_t size, tag_t tag)
{
	size_t req_size = size + DATA_OFFSET;
	struct alloc_entry *entry = NULL;
	struct list_head *free_node;

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
		if (rmem->alloc_size + req_size > RMEM_SIZE)
			return NULL;
		TEST_Z(entry = malloc(sizeof(struct alloc_entry)));
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
			prev_entry->size = end - start;
			list_delete(&entry->list);
			free(entry);
			entry = prev_entry;
		} else {
			struct list_head *last_free;
			start = prev_entry->start + prev_entry->size;
			entry->start = start;
			entry->size = end - start;
			last_free = entry->free_list.prev;
			list_insert(last_free, &entry->free_list);
		}
	} else {
		list_append(&rmem->free_list, &entry->free_list);
	}

	if (entry->list.next == &rmem->list) {
		list_delete(&entry->list);
		list_delete(&entry->free_list);
		rmem->alloc_size -= entry->size;
		free(entry);
		return NULL;
	}

	next_entry = entry_of_list(entry->list.next);
	if (next_entry->free) {
		end = next_entry->start + next_entry->size;
		list_delete(&next_entry->list);
		list_delete(&next_entry->free_list);
		free(next_entry);
	} else {
		end = next_entry->start;
	}
	entry->size = end - start;

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

void rmem_free(struct rmem_table *rmem, void *ptr)
{
	void *start = ptr - DATA_OFFSET;
	struct alloc_entry *entry;

	memcpy(&entry, start, sizeof(struct alloc_entry *));

	entry->free = 1;
	entry->tag = 0;
	//dump_free_list(rmem);
	entry = merge_free_blocks(rmem, entry);
	//dump_free_list(rmem);
	if (entry != NULL)
		set_free_ptrs(rmem, &entry->free_list);
	//dump_free_list(rmem);
}

void *rmem_lookup(struct rmem_table *rmem, tag_t tag)
{
	struct list_head *iter_node;
	struct alloc_entry *iter_entry;

	iter_node = rmem->list.next;

	while (iter_node != &rmem->list) {
		iter_entry = entry_of_list(iter_node);
		if (iter_entry->tag == tag)
			return iter_entry->start + DATA_OFFSET;
		iter_node = iter_node->next;
	}

	return NULL;
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
