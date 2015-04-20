#include "log.h"
#include "data/list.h"
#include "data/hash.h"
#include <stdlib.h>
#include <stdio.h>

#define HASH_SIZE 10000
#define CHECK_RETURN(ret) assert(ret == 0);

/*
 * Initialize log
 *
 * \param[in] cfg Log configuration
 * \returns handle to newly created log
 */ 
log_t* log_init(log_cfg_t cfg) {
    log_t* l = malloc(sizeof(log_t));
    
    if (l == NULL) {
        printf("Error allocation log structure");
        exit(-1);
    }

    l->tag = cfg.tag;
    l->translation_table = create_hash(HASH_SIZE);
    return l;
}

/*
 * Log commit updates the [client va] -> [remote va] translation table
 * commit is atomic because we assume the backup node does not die
 *
 * \pre log must be created
 * \param[in] l Handle to the log
 * \param[in] cr Commit record with commit information
 */
void log_commit(log_t* l, commit_record_t cr) {

    page_record_t* pr = cr.page_records;

    int npages = cr.npages;
    for (int i = 0; i < npages; ++i) {
        page_record_t pr = cr.page_records[i];

        // update hash table
        int* value = malloc(sizeof(uint64_t));
        CHECK_RETURN(value);

        insert_hash_item(l->translation_table, pr.local_va, value);
    }
}

void log_destroy(log_t* l) {
    destroy_hash(l->translation_table);
}

/*
 * Iterate over all elements in the hash table and
 * package them. These need to be sent for a new/recovered node
 *
 * \param[in] l Handle to the log to be recovered
 */
void log_recover(log_t* l) {
    hash_iterator_t it = begin_hash(l->translation_table);

    while (!is_iterator_null(it)) {
        uint64_t key = hash_iterator_key(it);
        uint64_t value = *(uint64_t*)hash_iterator_value(it);

        next_hash_iterator(it);
    }
}

int main() {

}

