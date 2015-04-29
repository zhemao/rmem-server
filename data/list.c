#include "list.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef struct list_node *list_node_t;

typedef struct list_node 
{
    list_node_t prev;
    list_node_t next;

    void *data;
} list_node;

struct list 
{
    int size;

    list_node_t head;
    list_node_t tail;
};

/*
 * STATIC / PRIVATE METHODS
 */ 

static
struct list_node* alloc_node(list_t list) 
{
    list_node* node = 0;
    node = (list_node*)malloc(sizeof(list_node));
    assert(node);
    node->prev = node->next = node->data = 0;
    return node;
}

static
list_node_t create_list_node(list_t list, void *data) 
{
    list_node_t node = alloc_node(list);
    assert(node);

    node->data = data;
    node->prev = node->next = 0;

    return node;
}

inline static
void free_node(list_t list, list_node_t node) 
{
    free(node);
}

/*
 * PUBLIC METHODS
 */ 

list_t list_create() 
{
    list_t list = 0;
    list = (list_t)malloc(sizeof(struct list));
    memset(list, 0, sizeof(struct list));
    assert(list);

    return list;
}


void list_push_back(list_t list, void* data) 
{
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

void list_destroy(list_t* list) 
{
    list_node_t node = (*list)->head;

    while (node) {
        list_node_t next = node->next;
        free_node(*list, node);
        node = next;
    }

    free(*list);
    *list = 0;
}

void* list_pop_back(list_t list) 
{
    assert(list->size);

    list_node_t node = list->tail;
    list->tail = list->tail->prev;

    if (list->tail) {
        list->tail->next = 0;
    } else {
        list->head = 0;
    }

    void* data = node->data;

    free_node(list, node);
    list->size--;

    return data;
}

void* list_pop_front(list_t list) 
{
    assert(list->size);

    list_node_t node = list->head;
    list->head = node->next;
    if (list->head)
        list->head->prev = 0;
    else list->tail = 0;

    void* data = node->data;

    free_node(list, node);
    list->size--;

    return data;
}

inline
int list_size(list_t list) 
{
    return list->size;
}

inline
list_iterator_t list_begin(list_t list) 
{
    return list->head;
}

void* list_get_value(list_iterator_t it) 
{
    return it->data;
}

inline
bool list_is_end(list_iterator_t it) 
{
    return it == 0;
}

list_iterator_t list_next_iterator(list_iterator_t it) 
{
    return it->next;
}

void list_delete_node(list_t list, list_iterator_t it) 
{
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

