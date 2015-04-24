#ifndef _HASH_H_
#define _HASH_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct hash *hash_t;
typedef struct hash_iterator* hash_iterator_t;

hash_t create_hash(int size);
void destroy_hash(hash_t);

void insert_hash_item(hash_t, uint64_t key, void* value);
void delete_hash_item(hash_t, uint64_t key);

void* get_item(hash_t, uint64_t key);
int hash_elements(hash_t);

hash_iterator_t begin_hash(hash_t);
void next_hash_iterator(hash_iterator_t);
uint64_t hash_iterator_key(hash_iterator_t);
void* hash_iterator_value(hash_iterator_t);
void delete_hash_iterator(hash_iterator_t);
bool is_iterator_null(hash_iterator_t);

#endif // _HASH_H_
