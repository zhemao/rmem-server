#include "ramc_transactions.h"

#include <assert.h>
#include <RamCloud.h>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <OptionParser.h>

#define CHUNK_NAME_SIZE (TXN_NAME_SIZE+100) // seems about right
#define EIGHT_MBS (8*1024*1024)
#define CHUNK_SIZE EIGHT_MBS
#define SIZE_FOR_CHUNK_PTRS (EIGHT_MBS - sizeof(uint32_t))

typedef struct value_format {
    uint32_t num_chunks;
    char txn_name[TXN_NAME_SIZE];
    uint32_t txn_counter;
} value_format_t;

using namespace RAMCloud;

static RamCloud* client;

std::string int_to_str(int n)
{
    std::ostringstream oss;
    oss<<n;
    return oss.str();
}

bool ramc_txn_module_init(int argc, char* argv[])
{
    try {
        Context context(false);

        OptionsDescription clientOptions("Client");
        OptionParser optionParser(clientOptions, argc, argv);

        std::string locator = optionParser.options.getCoordinatorLocator();
        client = new RamCloud(&context, locator.c_str(),
                optionParser.options.getClusterName().c_str());
    } catch (RAMCloud::ClientException& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        return false;
    } catch (RAMCloud::Exception& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        return false;
    }
    return true;
}

ramc_transaction_t* create_txn(char* name)
{
    ramc_transaction_t* txn = (ramc_transaction_t*)malloc(sizeof(ramc_transaction_t));
    assert(txn);

    strcpy(txn->name, name);
    txn->counter = 0;
        
    txn->table_id = client->createTable(txn->name);
    txn->table_id = client->getTableId(txn->name);

    return txn;
}

/*
 * This method saves the data in 'value' in chunks
 * of 8MBs (RAMCloud value limit)
 * these chunks are named with txn->name + txn->counter + index
 * Once the chunks are made an atomic put is committed with pointers
 * to the chunks
 */
bool commit_data(ramc_transaction_t* txn, char* value, int size)
{

    assert(size >= 0);

    int num_chunks = size / CHUNK_SIZE + 
        (size % CHUNK_SIZE != 0);

    assert(num_chunks > 0);

    // we need to do this in two steps:
    // 1) save chunks to some shadow memory
    // 2) atomically commit table of pointers

    value_format_t final_value;
    final_value.num_chunks = num_chunks;
    strcpy(final_value.txn_name, txn->name);
    final_value.txn_counter = txn->counter;

    // save chunks
    for (int i = 0; i < num_chunks; ++i) {
        char chunk_name[CHUNK_NAME_SIZE];
        strcpy(chunk_name, txn->name);
        strcat(chunk_name, int_to_str(txn->counter).c_str());
        strcat(chunk_name, int_to_str(i).c_str());
        client->write(txn->table_id, chunk_name, 
                strlen(chunk_name), value, size);
    }
       
    // atomically commit table of pointers 
    client->write(txn->table_id, txn->name,
                strlen(txn->name), (char*)&final_value, sizeof(value_format_t));

    txn->counter++;
}


