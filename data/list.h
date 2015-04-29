#ifndef _LIST_H_
#define _LIST_H_

#include <stdbool.h>

typedef struct list *list_t;
typedef struct list_node *list_iterator_t;

list_t list_create();
void list_destroy(list_t*);

void list_push_back(list_t, void*);
void* list_pop_back(list_t);
void* list_pop_front(list_t);

list_iterator_t list_begin(list_t);
void* list_get_value(list_iterator_t);
bool list_is_end(list_iterator_t);
list_iterator_t list_next_iterator(list_iterator_t);
void list_delete_node(list_t list, list_iterator_t);

int list_size(list_t);

#endif // _LIST_H_


