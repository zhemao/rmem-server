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

hash_t create_hash(int size) {
    hash_t new_hash = (hash_t)malloc(sizeof(hash_t));

    new_hash->elements = 0;
    new_hash->size = size;
    new_hash->hash_chains = malloc(sizeof(list_t) * size);
    assert(new_hash->hash_chains);

    // clear chain ptrs
    memset(new_hash->hash_chains, 0, sizeof(list_t) * size);

    return new_hash;
}

static
int hash_key(hash_t h, uint64_t key) {
    assert(h);
    return key % h->size;
}

void destroy_hash(hash_t h) {
    if (!h || !h->hash_chains)
        return;

    int i;
    for (i = 0; i < h->size; ++i) {
        if (h->hash_chains[i])
            destroy_list(&h->hash_chains[i]);
    }

    free(h->hash_chains);
    free(h);
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
    list_iterator_t list_it = begin_list(h->hash_chains[hash_index]);
    hash_node_t node;
    while (!is_end(list_it) && (node = get_value(list_it))) {
        if (node->key == key) {
            *node_out = list_it;
            return;
        }
        list_it = next_iterator(list_it);
    }

    *node_out = 0;
    return;
}

void insert_hash_item(hash_t h, uint64_t key, void* value) {
    assert(h);

    //LOG(0, ("inserting item\n"));
    int hash_index = hash_key(h, key);

    //LOG(0, ("key hashed\n"));
    if (!h->hash_chains[hash_index]) {
        //LOG(0, ("creating chain list\n"));
        h->hash_chains[hash_index] = create_list(false);
        assert(h->hash_chains[hash_index]);
    }
    list_t list = h->hash_chains[hash_index];

    hash_node_t node = (hash_node_t)malloc(sizeof(hash_node_t));
    //LOG(0, ("created hash node: 0x%lx\n", node));
    node->key = key;
    node->value = value;

    push_back_list(list, node);
    h->elements++;
}

void delete_hash_item(hash_t h, uint64_t key) {
    assert(h);

    list_iterator_t node;
    list_t node_list;

    get_item_node(h, key, &node, &node_list);
    if (!node)
        return;

    hash_node_t hash_node = get_value(node);
    free(node);

    delete_list_node(node_list, node);
    h->elements--;
}

void* get_item(hash_t h, uint64_t key) {
    assert(h);

    list_iterator_t node;
    list_t node_list;
    get_item_node(h, key, &node, &node_list);
    if (!node) // not found
        return 0;

    hash_node_t hash_node = (hash_node_t)get_value(node);
    return hash_node->value;
}

int hash_elements(hash_t h) {
    assert(h);
    return h->elements;
}

static inline
void clear_iterator(hash_iterator_t iterator) {
    iterator->hash_node = iterator->chain_list_index = iterator->next = 0;
}


void delete_hash_iterator(hash_iterator_t iterator) {
    free(iterator);
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
        if (h->hash_chains[i] && size_list(h->hash_chains[i]) > 0) {
            iterator->chain_list_index = i;

            list_iterator_t it = begin_list(h->hash_chains[i]);
            assert(it && get_value(it));

            // iterator->next may be a null pointer 
            // even though there is a next hash node
            iterator->next = next_iterator(it); 
            iterator->hash_node = get_value(it);
            return 0;
        }
    }

    clear_iterator(iterator);
    return 1;
}

bool is_iterator_null(hash_iterator_t iterator) {
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

hash_iterator_t begin_hash(hash_t h) {
    assert(h);
    hash_iterator_t iterator = (hash_iterator_t)malloc(sizeof(hash_iterator_t));

    iterator->hash = h;
    clear_iterator(iterator);
    if (h->elements != 0) {
        int ret = find_next_node(0, iterator);
    }
    return iterator;
}

void next_hash_iterator(hash_iterator_t iterator) {
    //LOG(0, ("next_h_it iterator->next: 0x%lx\n", iterator->next));
    if (iterator->next) { // there are more nodes in this chain list
        iterator->hash_node = get_value(iterator->next);
        iterator->next = next_iterator(iterator->next);
    } else { // need to find next chain list
        find_next_node(iterator->chain_list_index + 1, iterator);
    }
}

