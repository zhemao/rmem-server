#ifndef _LIST_H_
#define _LIST_H_

#include <stdbool.h>
#include <assert.h>

typedef struct list *list_t;
typedef struct list_node *list_iterator_t;

list_t create_list();
void destroy_list(list_t*);

void push_back_list(list_t, void*);
void* pop_front(list_t);

list_iterator_t begin_list(list_t);
void* get_value(list_iterator_t);
bool is_end(list_iterator_t);
list_iterator_t next_iterator(list_iterator_t);
void delete_list_node(list_t list, list_iterator_t);

int size_list(list_t);

#endif // _LIST_H_


