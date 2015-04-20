#ifndef _LOG_H_
#define _LOG_H_

#include <stdint.h>
#include "data/list.h"
#include "data/hash.h"

typedef struct page_record {
    uint64_t local_va;
    uint64_t remote_va;
} page_record_t;

typedef struct commit_record {
    int npages;
    page_record_t* page_records;
} commit_record_t;

typedef struct log_cfg {
    uint64_t tag;
} log_cfg_t;

typedef struct log {
    uint64_t tag; // uniquely identifies this log
    hash_t translation_table; 
} log_t;

log_t* log_init(log_cfg_t cfg);
void log_commit(log_t* l, commit_record_t cr);
void log_destroy(log_t* l);
void log_recover(log_t*);

#endif // _LOG_H_

