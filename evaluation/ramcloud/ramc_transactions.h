#ifndef _RAMC_TRANSACTIONS_H_
#define _RAMC_TRANSACTIONS_H_

#define TXN_NAME_SIZE 1000

#include <stdint.h>

typedef struct ramc_transaction {
    char name[TXN_NAME_SIZE];
    int counter;
    uint64_t table_id;
} ramc_transaction_t;


bool ramc_txn_module_init();
ramc_transaction_t* create_txn(char* name);
bool commit_data(ramc_transaction_t*, char* value, int size);

#endif // _RAMC_TRANSACTIONS_H_


