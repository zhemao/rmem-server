#include "list.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef struct list_node *list_node_t;

int nnodes = 0;

typedef struct list_node {
    list_node_t prev;
    list_node_t next;

    void *data;
} list_node;

struct list {
    int size;

    list_node_t head;
    list_node_t tail;
};

static
struct list_node* alloc_node(list_t list) {
    list_node* node = 0;
    node = (list_node*)malloc(sizeof(list_node));
    assert(node);
    node->prev = node->next = node->data = 0;
    return node;
}

list_t create_list() {
    list_t list = 0;
    list = (list_t)malloc(sizeof(struct list));
    memset(list, 0, sizeof(struct list));
    assert(list);

    return list;
}

static
list_node_t create_list_node(list_t list, void *data) {
    list_node_t node = alloc_node(list);
    assert(node);

    node->data = data;
    node->prev = node->next = 0;

    return node;
}

void push_back_list(list_t list, void* data) {
    list_node_t node = create_list_node(list, data);

    if (list->size == 0) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        node->prev = list->tail;
        node->next = 0;
        list->tail = node;
    }

    list->size++;
}

inline 
void free_node(list_t list, list_node_t node) {
    free(node);
}

void destroy_list(list_t* list) {
    list_node_t node = (*list)->head;

    while (node) {
        list_node_t next = node->next;
        free_node(*list, node);
        node = next;
    }

    free(*list);
    *list = 0;
}

void* pop_front(list_t list) {
    assert(list->size);

    list_node_t node = list->head;
    list->head = node->next;
    if (list->head)
        list->head->prev = 0;

    void* data = node->data;

    free_node(list, node);
    list->size--;

    return data;
}

inline
int size_list(list_t list) {
    return list->size;
}

inline
list_iterator_t begin_list(list_t list) {
    return list->head;
}

void* get_value(list_iterator_t it) {
    return it->data;
}

inline
bool is_end(list_iterator_t it) {
    return it == 0;
}

list_iterator_t next_iterator(list_iterator_t it) {
    return it->next;
}

void delete_list_node(list_t list, list_iterator_t it) {
    list->size--;

    list_node_t node = (list_node_t) it;
    list_node_t next = node->next;
    list_node_t prev = node->prev;

    free_node(list, node);

    if (prev)
        prev->next = next;
    else {
        list->head = next;
    }
}

