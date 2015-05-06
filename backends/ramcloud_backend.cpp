#include <assert.h>
#include "../common.h"
#include "../utils/error.h"
#include <RamCloud.h>
#include <OptionParser.h>
#include <map>

#define ONE_MB (1024*1024)
#define EIGHT_MB (8*ONE_MB)
#define VALUE_MAX_SIZE ONE_MB
#define TAG_ENTRIES_SIZE \
        ((VALUE_MAX_SIZE - sizeof(uint32_t)) / sizeof(tag_entry_t))
#define KEY_MAX_SIZE 6
#define TABLE_NAME "T"
#define MAIN_ENTRY_KEY "M"
#define TAG_TO_MAIN_KEY(tag) int_to_str(tag)
#define TAG_TO_SHADOW_KEY(tag) (TAG_TO_MAIN_KEY(tag)+"s")

extern "C" {
#include "ramcloud_backend.h"
}

using namespace RAMCloud;

typedef std::map<int,std::string> tag_map;

typedef struct ramcloud_data {
    uint64_t table_id;
    RamCloud* client;
    tag_map* tag_to_key;
    std::map<int, bool>* tag_written;
} ramcloud_data_t;

typedef struct tag_entry {
    uint32_t tag;
    char key[KEY_MAX_SIZE];
} tag_entry_t;

typedef struct rc_main_table_t {
    uint32_t num_tags;
    struct tag_entry tag_entries[TAG_ENTRIES_SIZE];
} rc_main_table_t;

/*
 * PRIVATE /STATIC METHODS
 */ 

static
std::string int_to_str(int n)
{
    std::ostringstream oss;
    oss<<n;
    return oss.str();
}

static
std::string shadow_key_from_main(const std::string& key) 
{
    assert(key.size() > 0);

    if (key[key.size() - 1] == 's') {
        return key.substr(0, key.size() - 1);
    } else {
        return key + "s";
    }
}


/*
 * PUBLIC METHODS
 */ 

void rmc_disconnect(rmem_layer_t *rmem_layer)
{
    // TBD
}

uint64_t rc_malloc(rmem_layer_t *rmem_layer, size_t size, uint32_t tag)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<int,std::string>::iterator it = tag_to_key->find(tag);

    if (it == tag_to_key->end()) {
        tag_to_key->operator[](tag) = int_to_str(tag);
    }

    return 1;
}

int rc_put(rmem_layer_t *rmem_layer, uint32_t tag,
        void *src, void *data_mr, size_t size)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<int,std::string>::iterator it = tag_to_key->find(tag);

    if (it == tag_to_key->end()) {
        tag_to_key->insert(std::make_pair(tag, TAG_TO_MAIN_KEY(tag)));
    } else {
    }

    it = tag_to_key->find(tag);
    std::string key = it->second;
    std::string write_key = key;

    uint64_t table_id = data->table_id;

    assert(size < VALUE_MAX_SIZE);
    fprintf(stderr, "rc_put4 tag: %u key:%s table_id:%lu key_size:%lu size:%lu\n", tag, write_key.c_str(), table_id, write_key.size(), size);

    data->client->write(table_id, write_key.c_str(), write_key.size(), src, size);
    data->tag_written->operator[](tag) = true;

    return 0;
}

int rc_get(rmem_layer_t *rmem_layer, void *dst, void *data_mr,
        uint32_t tag, size_t size)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<int,std::string>::iterator it = tag_to_key->find(tag);
    CHECK_ERROR(it == tag_to_key->end(),
            ("Error: did not find tag %d in tag_to_key map\n", tag));
    
    std::string key = it->second;
    uint64_t table_id = data->table_id;

    fprintf(stderr, "rc_get table id: %lu key: %s\n", table_id, key.c_str());

    Buffer buffer;
    data->client->read(table_id, key.c_str(), key.size(), &buffer);

    const char* bufferString = static_cast<const char*>(
            buffer.getRange(0, buffer.size()));
    
    memcpy(dst, bufferString, size);

    return 0;
}

int rc_free(rmem_layer_t *rmem_layer, uint32_t tag)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;

    std::map<int,std::string>::iterator it = tag_to_key->find(tag);

    if (it != tag_to_key->end()) {

        std::string key = it->second;
        uint64_t table_id = data->table_id;

        data->client->remove(table_id, key.c_str(), key.size());

        tag_to_key->erase(it);
    }

    return 0;
}

/*
 * we currently commit everything
 */
int rc_atomic_commit(rmem_layer_t* rmem_layer, uint32_t* tags_src,
        uint32_t* tags_dst, uint32_t* tags_size,
        uint32_t num_tags)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    uint64_t table_id = data->table_id;

    std::vector<std::string> to_be_removed;

    /*
     * For each tag to be commited (shadow->final destination)
     * We write it to a new place, update atomically the main table in RC
     * and remove the old entries
     */
    for (unsigned int i = 0; i < num_tags; ++i) {
        int tag_src = tags_src[i];
        int tag_dst = tags_dst[i];

        std::map<int, std::string>::iterator src_key_it = tag_to_key->find(tag_src);
        std::map<int, std::string>::iterator dst_key_it = tag_to_key->find(tag_dst);

        // both src and dest tags should exist
        assert(src_key_it != tag_to_key->end());
        assert(dst_key_it != tag_to_key->end());

        // this is the old key, because we are going to write this to a new place
        std::string old_dst_key = dst_key_it->second;
      
        // new kwy 
        // add 's' or remove, alternating
        std::string new_dst_key = shadow_key_from_main(old_dst_key); 

        // get data from src (shadow)
        Buffer buffer;
        data->client->read(table_id, src_key_it->second.c_str(), src_key_it->second.size(), &buffer);
            
        const char* bufferString = static_cast<const char*>(
                buffer.getRange(0, buffer.size()));
        data->client->write(table_id, new_dst_key.c_str(), 
                new_dst_key.size(), bufferString, buffer.size());

        // update the tag-to-key map
        tag_to_key->operator[](tag_dst) = new_dst_key;

        // this is going to be removed after we commit the new main table entry
        // cant be done before otherwise we may lose pages
        to_be_removed.push_back(old_dst_key);
    }

    // main table to be populated
    rc_main_table_t* main_table = new rc_main_table_t;
    main_table->num_tags = tag_to_key->size();

    // traverse all tags, not just those committed
    // all need to be saved in main table
    int i = 0;
    for (std::map<int, std::string>::iterator it = tag_to_key->begin();
            it != tag_to_key->end(); ++it, ++i) {
        int tag = it->first;
        std::string key = it->second;
        main_table->tag_entries[i].tag = tag;
        strcpy(main_table->tag_entries[i].key, key.c_str());
    }
    data->client->write(table_id, MAIN_ENTRY_KEY,
            strlen(MAIN_ENTRY_KEY), (char*)main_table, sizeof(rc_main_table_t));

    for (std::vector<std::string>::iterator it = to_be_removed.begin();
            it != to_be_removed.end(); ++it) {
        data->client->remove(table_id, it->c_str(), it->size());
    }

    delete main_table;

    return 0;
}

void *rc_register_data(rmem_layer_t* rmem_layer, void *data, size_t size)
{
    return (void*)1;
}

void rc_deregister_data(rmem_layer_t* rmem_layer, void *data)
{
}

void ramcloud_connect(rmem_layer_t *rmem_layer, char *host, char *port)
{
    try {
        Context* context = new Context(false);

        OptionsDescription* clientOptions = new OptionsDescription("Client");

        std::string arg = std::string("infrc:host=") + host + ",port=" + port;
        int argc = 3;
        char *argv[] = {(char *) "test", (char *) "-C",
            const_cast<char*>(arg.c_str())};

        OptionParser* optionParser = new OptionParser(*clientOptions, argc, argv);


        std::string locator = optionParser->options.getCoordinatorLocator();
        
        fprintf(stderr, "Connecting to %s locator: %s\n", arg.c_str(), locator.c_str());

        RamCloud* client = new RamCloud(context, locator.c_str(),
                optionParser->options.getClusterName().c_str());

        uint64_t table_id;
        ramcloud_data_t* data = new ramcloud_data_t;
        tag_map* tag_to_key_map = new std::map<int, std::string>();
        std::map<int,bool>* tag_written = new std::map<int, bool>();

        rc_main_table_t* main_table = new rc_main_table_t;
        try {
            table_id = client->getTableId(TABLE_NAME);

            // table exists
            // we have to load our tag_to_key map from main table
            Buffer buffer;
            client->read(table_id, MAIN_ENTRY_KEY, strlen(MAIN_ENTRY_KEY), &buffer);

            const char* bufferString = static_cast<const char*>(
                    buffer.getRange(0, buffer.size()));
            memcpy(main_table, bufferString, sizeof(*main_table));

            for (unsigned int i = 0; i < main_table->num_tags; ++i) {
                int tag = main_table->tag_entries[i].tag;
                char* key = main_table->tag_entries[i].key;

                tag_to_key_map->operator[](tag) = std::string(key);
            }
        } catch (TableDoesntExistException& e) {
            // table does not exist
            // create table and initialize main table
            memset(main_table, 0, sizeof(*main_table));

            client->createTable(TABLE_NAME);
            table_id = client->getTableId(TABLE_NAME);

            fprintf(stderr,"Table does not exist. id: %lu\n", table_id);

            // write main table
            // this basically tells this layer where each tag's data lives
            client->write(table_id, MAIN_ENTRY_KEY, 
                    strlen(MAIN_ENTRY_KEY), main_table, sizeof(*main_table));
        } catch (...) {
            assert(0);
        }

        data->client = client;
        data->tag_written = tag_written;
        data->tag_to_key = tag_to_key_map;
        data->table_id = table_id;
        rmem_layer->layer_data = data;

    } catch (RAMCloud::ClientException& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        assert(0);
    } catch (RAMCloud::Exception& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        assert(0);
    }   
}


extern "C" {
    rmem_layer_t* create_ramcloud_layer()
    {
        rmem_layer_t* layer = (rmem_layer_t*)
            malloc(sizeof(rmem_layer_t));
        CHECK_ERROR(layer == 0,
                ("Failure: Error allocating layer struct\n"));

        memset(layer, 0, sizeof(rmem_layer_t));

        layer->connect = ramcloud_connect;
        layer->disconnect = rmc_disconnect;
        layer->malloc = rc_malloc;
        layer->free = rc_free;
        layer->put = rc_put;
        layer->get = rc_get;
        layer->atomic_commit = rc_atomic_commit;
        layer->register_data = rc_register_data;
        layer->deregister_data = rc_deregister_data;

        return layer;
    }
}
