#include "hash.h"
#include "list.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct hash_node* hash_node_t;

typedef struct hash_node {
    uint64_t key;
    void* value;
} hash_node;

typedef struct hash_iterator {
    int chain_list_index;
    hash_node_t hash_node;
    hash_t hash;
    list_iterator_t next;
} hash_iterator;

typedef struct hash {
    list_t* hash_chains;
    int size; // number of buckets
    int elements; // number of elements added
} hash;

/*
 * PRIVATE / STATIC METHODS
 */ 

static
int hash_key(hash_t h, uint64_t key) {
    assert(h);
    return key % h->size;
}

// Ugly
static
void get_item_node(hash_t h, uint64_t key, list_iterator_t* node_out, list_t* list_out) {
    if (!h || h->size == 0) {
        *node_out = 0;
        *list_out = 0;
        return;
    }

    int hash_index = hash_key(h, key);
    if (!h->hash_chains[hash_index]) {
        *node_out = 0;
        *list_out = 0;
        return;
    }

    *list_out = h->hash_chains[hash_index];
    list_iterator_t list_it = list_begin(h->hash_chains[hash_index]);
    hash_node_t node;
    while (!list_is_end(list_it) && (node = (hash_node*)list_get_value(list_it))) {
        if (node->key == key) {
            *node_out = list_it;
            return;
        }
        list_it = list_next_iterator(list_it);
    }

    *node_out = 0;
    return;
}

static 
void clear_iterator(hash_iterator_t iterator) {
    memset(iterator, 0, sizeof(struct hash_iterator));
}

/* Find next node starting from specific chain list index
 * Once next node is found initialize iterator
 * Return 0 if node is found, 1 otherwise
 */
static
int find_next_node(int chain_list_index, hash_iterator_t iterator) {
    int i = chain_list_index;
    hash_t h = iterator->hash;
    for (; i < h->size; ++i) {
        if (h->hash_chains[i] && list_size(h->hash_chains[i]) > 0) {
            iterator->chain_list_index = i;

            list_iterator_t it = list_begin(h->hash_chains[i]);
            assert(it && list_get_value(it));

            // iterator->next may be a null pointer 
            // even though there is a next hash node
            iterator->next = list_next_iterator(it); 
            iterator->hash_node = (hash_node_t)list_get_value(it);
            return 0;
        }
    }

    clear_iterator(iterator);
    return 1;
}

/*
 * PUBLIC METHODS
 */

hash_t hash_create(int size) {
    hash_t new_hash = (hash_t)malloc(sizeof(struct hash));
    assert(new_hash);

    new_hash->elements = 0;
    new_hash->size = size;
    new_hash->hash_chains = (list_t*)malloc(sizeof(list_t) * size);
    assert(new_hash->hash_chains);

    // clear chain ptrs
    memset(new_hash->hash_chains, 0, sizeof(list_t) * size);

    return new_hash;
}


void hash_destroy(hash_t h) {
    if (!h || !h->hash_chains)
        return;

    int i;
    for (i = 0; i < h->size; ++i) {
        if (h->hash_chains[i])
            list_destroy(&h->hash_chains[i]);
    }

    free(h->hash_chains);
    free(h);
}


void hash_insert_item(hash_t h, uint64_t key, void* value) {
    assert(h);

    //LOG(0, ("inserting item\n"));
    int hash_index = hash_key(h, key);

    //LOG(0, ("key hashed\n"));
    if (!h->hash_chains[hash_index]) {
        //LOG(0, ("creating chain list\n"));
        h->hash_chains[hash_index] = list_create();
        assert(h->hash_chains[hash_index]);
    }
    list_t list = h->hash_chains[hash_index];

    hash_node_t node = (hash_node_t)malloc(sizeof(hash_node));
    //LOG(0, ("created hash node: 0x%lx\n", node));
    node->key = key;
    node->value = value;

    list_push_back(list, node);
    h->elements++;
}

void hash_delete_item(hash_t h, uint64_t key) {
    assert(h);

    list_iterator_t node;
    list_t node_list;

    get_item_node(h, key, &node, &node_list);
    if (!node)
        return;

    hash_node_t hash_node = (hash_node_t)list_get_value(node);
    free(hash_node);

    list_delete_node(node_list, node);
    h->elements--;
}

void* hash_get_item(hash_t h, uint64_t key) {
    assert(h);

    list_iterator_t node;
    list_t node_list;
    get_item_node(h, key, &node, &node_list);
    if (!node) // not found
        return 0;

    hash_node_t hash_node = (hash_node_t)list_get_value(node);
    return hash_node->value;
}

int hash_num_elements(hash_t h) {
    assert(h);
    return h->elements;
}


void hash_delete_iterator(hash_iterator_t iterator) {
    free(iterator);
}

bool hash_is_iterator_null(hash_iterator_t iterator) {
    return iterator->hash_node == 0;
}

uint64_t hash_iterator_key(hash_iterator_t iterator) {
    assert(iterator->hash_node);
    //LOG(0, ("it hash_node: 0x%lx\n", iterator->hash_node));
    return iterator->hash_node->key;
}

void* hash_iterator_value(hash_iterator_t iterator) {
    assert(iterator->hash_node);
    return iterator->hash_node->value;
}

hash_iterator_t hash_begin(hash_t h) {
    assert(h);
    hash_iterator_t iterator = (hash_iterator_t)malloc(sizeof(struct hash_iterator));
    assert(iterator);

    clear_iterator(iterator);
    iterator->hash = h;
    if (h->elements != 0) {
        find_next_node(0, iterator);
    }
    return iterator;
}

void hash_next_iterator(hash_iterator_t iterator) {
    //LOG(0, ("next_h_it iterator->next: 0x%lx\n", iterator->next));
    if (iterator->next) { // there are more nodes in this chain list
        iterator->hash_node = (hash_node_t)list_get_value(iterator->next);
        iterator->next = list_next_iterator(iterator->next);
    } else { // need to find next chain list
        find_next_node(iterator->chain_list_index + 1, iterator);
    }
}

