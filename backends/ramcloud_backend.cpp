#include <assert.h>
#include "../common.h"
#include "../utils/error.h"
#include <RamCloud.h>
#include <OptionParser.h>
#include <map>
#include <algorithm>

#define ONE_MB (1024*1024)
#define EIGHT_MB (8*ONE_MB)
#define VALUE_MAX_SIZE (4*1024)
#define TAG_ENTRIES_SIZE \
        ((VALUE_MAX_SIZE - sizeof(uint32_t)) / sizeof(tag_entry_t))
#define KEY_MAX_SIZE 6
#define TABLE_NAME "T"
#define MAIN_ENTRY_KEY "M"
#define TAG_TO_MAIN_KEY(tag) int_to_str(tag)
#define TAG_TO_SHADOW_KEY(tag) (TAG_TO_MAIN_KEY(tag)+"s")
#define CHUNK_KEY(key, i) (key + "." + int_to_str(i))

extern "C" {
#include "ramcloud_backend.h"
}

using namespace RAMCloud;

typedef std::map<uint32_t,std::string> tag_map;

typedef struct ramcloud_data {
    uint64_t table_id;
    RamCloud* client;
    tag_map* tag_to_key;
    std::map<uint32_t, bool>* tag_written;
    std::map<uint32_t, size_t>* tag_to_size;
} ramcloud_data_t;

typedef struct tag_entry {
    uint32_t tag;
    char key[KEY_MAX_SIZE];
    size_t size;
} tag_entry_t;

typedef struct rc_main_table_t {
    uint32_t num_tags;
    struct tag_entry tag_entries[TAG_ENTRIES_SIZE];
} rc_main_table_t;

#define NS_PER_SEC (1000.0 * 1000.0 * 1000.0)
static inline double gettime(void)
{
    struct timespec ts; 

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }   

    return ts.tv_sec + ((double) ts.tv_nsec) / NS_PER_SEC;
}


// array of 0's
char* zero_data = 0;

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
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    //data->client->dropTable(TABLE_NAME);

    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;

    tag_to_size->clear();
    tag_to_key->clear();
}

uint64_t rc_malloc(rmem_layer_t *rmem_layer, size_t size, uint32_t tag)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    uint64_t table_id = data->table_id;
    tag_map* tag_to_key = data->tag_to_key;

    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;
    std::map<uint32_t,std::string>::iterator it = tag_to_key->find(tag);

    if (it == tag_to_key->end()) {
        tag_to_key->operator[](tag) = int_to_str(tag);
        tag_to_size->operator[](tag) = size;
    }

    // make sure this tag exists in ramcloud
    int64_t size_left = size;
    int i = 0;
    while (size_left > 0) {
        std::string write_chunk_key = CHUNK_KEY(int_to_str(tag), i);
        int64_t size_to_write = std::min(size_left, (int64_t)VALUE_MAX_SIZE);

#ifdef RAMC_DEBUG 
        fprintf(stderr, "rc_malloc: writing table_id: %ld key: %s size: %d\n", 
                table_id, write_chunk_key.c_str(), size_to_write);
#endif
        data->client->write(table_id, write_chunk_key.c_str(), 
                write_chunk_key.size(), (char*)zero_data, size_to_write);
        ++i;
        size_left -= VALUE_MAX_SIZE;
    }

    return 1;
}

int rc_put(rmem_layer_t *rmem_layer, uint32_t tag,
        void *src, void *data_mr, size_t size)
{
    double put_start_time = gettime();
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t,std::string>::iterator it = tag_to_key->find(tag);

    if (it == tag_to_key->end()) {
        assert(0); // we should have added this back in rc_malloc
        //tag_to_key->insert(std::make_pair(tag, TAG_TO_MAIN_KEY(tag)));
    } else {
    }

    it = tag_to_key->find(tag);
    std::string key = it->second;
    std::string write_key = key;

    uint64_t table_id = data->table_id;

#ifdef RAMC_DEBUG
    fprintf(stderr, "rc_put4 tag: %u key:%s table_id:%lu key_size:%lu size:%lu\n", 
            tag, write_key.c_str(), table_id, write_key.size(), size);
#endif

    int64_t size_left = size;
    int i = 0;
    while (size_left > 0) {
        std::string write_chunk_key = CHUNK_KEY(write_key, i);
        int64_t size_to_write = std::min(size_left, (int64_t)VALUE_MAX_SIZE);

#ifdef RAMC_DEBUG
        fprintf(stderr, "rc_put writing key: %s size: %d\n", write_chunk_key.c_str(), size_to_write);
#endif
        data->client->write(table_id, write_chunk_key.c_str(), 
                write_chunk_key.size(), (char*)src + i * VALUE_MAX_SIZE, size_to_write);
        ++i;
        size_left -= VALUE_MAX_SIZE;
    }

    data->tag_written->operator[](tag) = true;

    double put_end_time = gettime();
    //fprintf(stderr, "rc_put time: %lf\n", (put_end_time - put_start_time));

    return 0;
}

int rc_get(rmem_layer_t *rmem_layer, void *dst, void *data_mr,
        uint32_t tag, size_t size)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    uint64_t table_id = data->table_id;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t,std::string>::iterator it = tag_to_key->find(tag);
    CHECK_ERROR(it == tag_to_key->end(),
            ("Error: did not find tag %d in tag_to_key map\n", tag));

    std::string key = it->second;

#ifdef RAMC_DEBUG
    fprintf(stderr, "rc_get table id: %lu key: %s size: %u\n", table_id, key.c_str(), size);
#endif

    int64_t size_left = size;
    int i = 0;
    while (size_left > 0) {
        std::string chunk_key = CHUNK_KEY(key, i);
        Buffer buffer;

        data->client->read(table_id, chunk_key.c_str(), 
                chunk_key.size(), &buffer);

#ifdef RAMC_DEBUG 
        fprintf(stderr, "rc_get client->read. table id: %lu chunk_key: %s size: %u\n", 
                table_id, chunk_key.c_str(), buffer.size());
#endif

        const char* bufferString = static_cast<const char*>(
                buffer.getRange(0, buffer.size()));

        memcpy((char*)dst + i * VALUE_MAX_SIZE, bufferString, buffer.size());

        size_left -= VALUE_MAX_SIZE;
        ++i;
    }

    return 0;
}

/*
 * Warning: free is not atomic
 */ 
int rc_free(rmem_layer_t *rmem_layer, uint32_t tag)
{
    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;

    std::map<uint32_t,std::string>::iterator it = tag_to_key->find(tag);

    tag_to_key->erase(it);
    tag_to_size->erase(tag_to_size->find(tag));

    return 0;
}

static
void write_tag_to_tag(ramcloud_data_t* data, uint32_t tag_src, uint32_t tag_dst)
{
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;
    uint64_t table_id = data->table_id;

    std::map<uint32_t, std::string>::iterator src_key_it = tag_to_key->find(tag_src);
    std::map<uint32_t, std::string>::iterator dst_key_it = tag_to_key->find(tag_dst);

    // both src and dest tags should exist
    assert(src_key_it != tag_to_key->end());
    assert(dst_key_it != tag_to_key->end());

    size_t src_size = tag_to_size->operator[](tag_src);
    size_t dst_size = tag_to_size->operator[](tag_dst);

    assert(src_size > 0 && dst_size > 0);

    // this is the old key, because we are going to write this to a new place
    std::string old_dst_key = dst_key_it->second;

    // new kwy 
    // add 's' or remove, alternating
    std::string new_dst_key = shadow_key_from_main(old_dst_key); 

    for (size_t i = 0; i * VALUE_MAX_SIZE < src_size; ++i) {
        std::string src_chunk_key = CHUNK_KEY(src_key_it->second, i);
        std::string dst_chunk_key = CHUNK_KEY(new_dst_key, i);

        Buffer buffer;
        data->client->read(table_id, src_chunk_key.c_str(), src_chunk_key.size(), &buffer);

        assert(buffer.size() <= VALUE_MAX_SIZE);

        const char* bufferString = static_cast<const char*>(
                buffer.getRange(0, buffer.size()));

#ifdef RAMC_DEBUG
        fprintf(stderr, "write_tag writing key: %s size: %d\n", dst_chunk_key.c_str(), buffer.size());
#endif

        data->client->write(table_id, dst_chunk_key.c_str(),
                dst_chunk_key.size(), bufferString, buffer.size());
    }
}

/*
 * we currently commit everything
 */

/*
 * We are commiting tags_dst by copying tafs_src to tags_dst
 * this means tags_dst should point to tags_src
 *
 */
int rc_atomic_commit(rmem_layer_t* rmem_layer, uint32_t* tags_src,
        uint32_t* tags_dst, uint32_t* tags_size,
        uint32_t num_tags)
{
    double commit_start_time = gettime();

    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;
    uint64_t table_id = data->table_id;

    std::vector<std::string> to_be_removed;

    /*
     * For each tag to be commited (shadow->final destination)
     * We write it to a new place, update atomically the main table in RC
     * and remove the old entries
     */
    
    double phase1_start_time = gettime();
    for (unsigned int i = 0; i < num_tags; ++i) {
        int tag_src = tags_src[i];
        int tag_dst = tags_dst[i];

        std::map<uint32_t, std::string>::iterator src_key_it = tag_to_key->find(tag_src);
        std::map<uint32_t, std::string>::iterator dst_key_it = tag_to_key->find(tag_dst);

        // both src and dest tags should exist
        assert(src_key_it != tag_to_key->end());
        assert(dst_key_it != tag_to_key->end());


        // this is the old key, because we are going to write this to a new place
        std::string old_dst_key = dst_key_it->second;

        // new key 
        // add 's' or remove, alternating
        std::string new_dst_key = shadow_key_from_main(old_dst_key); 

#ifdef RAMC_DEBUG   
        fprintf(stderr, "commiting tag_src: %d to tag_dst: %d\n", tag_src, tag_dst);
#endif
        double start_time = gettime();
        write_tag_to_tag(data, tag_src, tag_dst);
        double end_time = gettime();
        fprintf(stderr, "tag_to_tag time: %f\n", end_time-start_time);
        /*


        // get data from src (shadow)

        Buffer buffer;
        data->client->read(table_id, src_key_it->second.c_str(), src_key_it->second.size(), &buffer);

        const char* bufferString = static_cast<const char*>(
        buffer.getRange(0, buffer.size()));
        data->client->write(table_id, new_dst_key.c_str(), 
        new_dst_key.size(), bufferString, buffer.size());
         */

        // update the tag-to-key map
        tag_to_key->operator[](tag_dst) = new_dst_key;

        // this is going to be removed after we commit the new main table entry
        // cant be done before otherwise we may lose pages
        to_be_removed.push_back(old_dst_key);
    }
    double phase1_end_time = gettime();
    double phase2_start_time = gettime();

    // main table to be populated
    rc_main_table_t* main_table = new rc_main_table_t;
    main_table->num_tags = tag_to_key->size();

    // traverse all tags, not just those committed
    // all need to be saved in main table
    int i = 0;
    for (std::map<uint32_t, std::string>::iterator it = tag_to_key->begin();
            it != tag_to_key->end(); ++it, ++i) {
        int tag = it->first;
        std::string key = it->second;
        size_t tag_size = tag_to_size->operator[](tag);

        main_table->tag_entries[i].tag = tag;
        main_table->tag_entries[i].size = tag_size;
        strcpy(main_table->tag_entries[i].key, key.c_str());
    }
    double phase2_end_time = gettime();
    double phase3_start_time = gettime();
    data->client->write(table_id, MAIN_ENTRY_KEY,
            strlen(MAIN_ENTRY_KEY), (char*)main_table, sizeof(rc_main_table_t));
    double phase3_end_time = gettime();
    double phase4_start_time = gettime();

    //for (std::vector<std::string>::iterator it = to_be_removed.begin();
    //        it != to_be_removed.end(); ++it) {
    //    data->client->remove(table_id, it->c_str(), it->size());
    //}
    double phase4_end_time = gettime();

    delete main_table;
    
    double commit_end_time = gettime();
    fprintf(stderr, "commit time: %f phase1_time: %lf phase2_time: %lf"
            "phase3_time: %lf phase4_time: %lf\n", 
            commit_end_time-commit_start_time,
            phase1_end_time - phase1_start_time,
            phase2_end_time - phase2_start_time,
            phase3_end_time - phase3_start_time,
            phase4_end_time - phase4_start_time);

    return 0;
}

int rc_atomic_commit2(rmem_layer_t* rmem_layer, uint32_t* tags_src,
        uint32_t* tags_dst, uint32_t* tags_size,
        uint32_t num_tags)
{
    double commit_start_time = gettime();

    ramcloud_data_t* data = (ramcloud_data_t*)rmem_layer->layer_data;
    tag_map* tag_to_key = data->tag_to_key;
    std::map<uint32_t, size_t>* tag_to_size = data->tag_to_size;
    uint64_t table_id = data->table_id;

    std::vector<std::string> to_be_removed;

    /*
     * For each tag to be commited (shadow->final destination)
     * We write it to a new place, update atomically the main table in RC
     * and remove the old entries
     */
    
    double phase1_start_time = gettime();
    for (unsigned int i = 0; i < num_tags; ++i) {
        int tag_src = tags_src[i];
        int tag_dst = tags_dst[i];

        std::map<uint32_t, std::string>::iterator src_key_it = tag_to_key->find(tag_src);
        std::map<uint32_t, std::string>::iterator dst_key_it = tag_to_key->find(tag_dst);
        
        // both src and dest tags should exist
        assert(src_key_it != tag_to_key->end());
        assert(dst_key_it != tag_to_key->end());

        std::string src_key = src_key_it->second;
        std::string dst_key = dst_key_it->second;

        tag_to_key->operator[](tag_src) = dst_key;
        tag_to_key->operator[](tag_dst) = src_key;
    }
    double phase1_end_time = gettime();
    double phase2_start_time = gettime();

    // main table to be populated
    rc_main_table_t* main_table = new rc_main_table_t;
    main_table->num_tags = tag_to_key->size();

    // traverse all tags, not just those committed
    // all need to be saved in main table
    int i = 0;
    for (std::map<uint32_t, std::string>::iterator it = tag_to_key->begin();
            it != tag_to_key->end(); ++it, ++i) {
        int tag = it->first;
        std::string key = it->second;
        size_t tag_size = tag_to_size->operator[](tag);

        main_table->tag_entries[i].tag = tag;
        main_table->tag_entries[i].size = tag_size;
        strcpy(main_table->tag_entries[i].key, key.c_str());
    }
    double phase2_end_time = gettime();
    double phase3_start_time = gettime();
    data->client->write(table_id, MAIN_ENTRY_KEY,
            strlen(MAIN_ENTRY_KEY), (char*)main_table, sizeof(rc_main_table_t));
    double phase3_end_time = gettime();
    double phase4_start_time = gettime();

    //for (std::vector<std::string>::iterator it = to_be_removed.begin();
    //        it != to_be_removed.end(); ++it) {
    //    data->client->remove(table_id, it->c_str(), it->size());
    //}
    double phase4_end_time = gettime();

    delete main_table;
    
    double commit_end_time = gettime();
    fprintf(stderr, "commit time: %f phase1_time: %lf phase2_time: %lf"
            "phase3_time: %lf phase4_time: %lf\n", 
            commit_end_time-commit_start_time,
            phase1_end_time - phase1_start_time,
            phase2_end_time - phase2_start_time,
            phase3_end_time - phase3_start_time,
            phase4_end_time - phase4_start_time);

    return 0;
}

int rc_multi_malloc(rmem_layer_t* rmem_layer, uint64_t *addrs,
        uint64_t size, uint32_t *tags, uint32_t n)
{
    for (int i = 0; i < n; i++) {
        addrs[i] = rc_malloc(rmem_layer, size, tags[i]);
        if (addrs[i] == 0)
            return -1;
    }
    return 0;
}

int rc_multi_free(rmem_layer_t *rmem_layer, uint32_t *tags, uint32_t n)
{
    int res;
    for (int i = 0; i < n; i++) {
        res = rc_free(rmem_layer, tags[i]);
        if (res != 0)
            return res;
    }
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
    double connect_start_time = gettime();
    double connect_start_time1 = 0;
    double connect_start_time2 = 0;
    double connect_start_time3= 0;
    double connect_start_time4 = 0;
    double connect_start_time5 = 0;
    double connect_end_time1 = 0;
    double connect_end_time2 = 0;
    double connect_end_time3 = 0;
    double connect_end_time4 = 0;
    double connect_end_time5 = 0;
    try {
        double connect_start_time1 = gettime();
        Context* context = new Context(false);

        OptionsDescription* clientOptions = new OptionsDescription("Client");

        std::string arg = std::string("infrc:host=") + host + ",port=" + port;
        int argc = 3;
        char *argv[] = {(char *) "test", (char *) "-C",
            const_cast<char*>(arg.c_str())};

        OptionParser* optionParser = new OptionParser(*clientOptions, argc, argv);


        std::string locator = optionParser->options.getCoordinatorLocator();

        fprintf(stderr, "Connecting to %s locator: %s\n", arg.c_str(), locator.c_str());
        connect_end_time1 = gettime();
        connect_start_time2 = gettime();

        RamCloud* client = new RamCloud(context, locator.c_str(),
                optionParser->options.getClusterName().c_str());
        connect_end_time2 = gettime();
        connect_start_time3 = gettime();

        uint64_t table_id;
        ramcloud_data_t* data = new ramcloud_data_t;
        tag_map* tag_to_key_map = new std::map<uint32_t, std::string>();
        std::map<uint32_t, size_t>* tag_to_size = new std::map<uint32_t, size_t>();
        std::map<uint32_t,bool>* tag_written = new std::map<uint32_t, bool>();

        rc_main_table_t* main_table = new rc_main_table_t;
        try {
            table_id = client->getTableId(TABLE_NAME);
            connect_end_time3 = gettime();
            connect_start_time4 = gettime();

            fprintf(stderr, "Table exists\n");

            // table exists
            // we have to load our tag_to_key map from main table
            Buffer buffer;
            client->read(table_id, MAIN_ENTRY_KEY, strlen(MAIN_ENTRY_KEY), &buffer);
            connect_end_time4 = gettime();
            connect_start_time5 = gettime();

            const char* bufferString = static_cast<const char*>(
                    buffer.getRange(0, buffer.size()));
            memcpy(main_table, bufferString, sizeof(*main_table));

            for (unsigned int i = 0; i < main_table->num_tags; ++i) {
                int tag = main_table->tag_entries[i].tag;
                char* key = main_table->tag_entries[i].key;
                size_t tag_size = main_table->tag_entries[i].size;

                tag_to_key_map->operator[](tag) = std::string(key);
                tag_to_size->operator[](tag) = tag_size;

                //fprintf(stderr, "recovered tag: %d key: %s size: %d\n", tag, key, tag_size);
            }
            connect_end_time5 = gettime();
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
        data->tag_to_size = tag_to_size;
        data->table_id = table_id;
        rmem_layer->layer_data = data;

    } catch (RAMCloud::ClientException& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        assert(0);
    } catch (RAMCloud::Exception& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        assert(0);
    }   
    double connect_end_time = gettime();
    fprintf(stderr, "connect times: 1:%lf 2:%lf 3:%lf 4:%lf 5:%lf\n", 
            connect_end_time - connect_start_time,
            connect_end_time1 - connect_start_time1,
            connect_end_time2 - connect_start_time2,
            connect_end_time3 - connect_start_time3,
            connect_end_time4 - connect_start_time4,
            connect_end_time5 - connect_start_time5);
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
        layer->multi_malloc = rc_multi_malloc;
        layer->multi_free = rc_multi_free;
        layer->register_data = rc_register_data;
        layer->deregister_data = rc_deregister_data;

        // initialize  
        zero_data = new char[VALUE_MAX_SIZE];
        assert(zero_data);
        memset(zero_data, 0, VALUE_MAX_SIZE * sizeof(char));

        return layer;
    }
}
