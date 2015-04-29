#ifndef _HASH_H_
#define _HASH_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct hash *hash_t;
typedef struct hash_iterator* hash_iterator_t;

hash_t hash_create(int size);
void hash_destroy(hash_t);

void hash_insert_item(hash_t, uint64_t key, void* value);
void hash_delete_item(hash_t, uint64_t key);

void* hash_get_item(hash_t, uint64_t key);
int hash_num_elements(hash_t);

hash_iterator_t hash_begin(hash_t);
void hash_next_iterator(hash_iterator_t);
uint64_t hash_iterator_key(hash_iterator_t);
void* hash_iterator_value(hash_iterator_t);
void hash_delete_iterator(hash_iterator_t);
bool hash_is_iterator_null(hash_iterator_t);

#endif // _HASH_H_
