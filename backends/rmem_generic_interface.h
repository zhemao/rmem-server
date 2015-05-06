#ifndef _BACKUP_INTERFACE_H_
#define _BACKUP_INTERFACE_H_

#include <rdma/rdma_cma.h>

typedef struct rmem_layer rmem_layer_t;

typedef void (*rmem_connect_f)(rmem_layer_t*, char*, char*);
typedef void (*rmem_disconnect_f)(rmem_layer_t*);

typedef uint64_t (*rmem_malloc_f)(rmem_layer_t*, 
        size_t , uint32_t );
typedef int (*rmem_free_f)(rmem_layer_t*, uint32_t);

typedef int (*rmem_put_f)(rmem_layer_t*, uint32_t , 
        void *, void *, size_t);
typedef int (*rmem_get_f)(rmem_layer_t*, void *, 
        void *, uint32_t, size_t);

typedef int (*rmem_atomic_commit_f)(rmem_layer_t*, 
        uint32_t*, uint32_t*, uint32_t*, uint32_t);

typedef void* (*rmem_register_data_f)(rmem_layer_t*,
        void*, size_t size);
typedef void (*rmem_deregister_data_f)(rmem_layer_t*, void*);

typedef rmem_layer_t* (*create_rmem_layer_f)();

typedef struct data_record {
} data_record_t;

typedef struct rmem_layer {
    rmem_connect_f connect;
    rmem_disconnect_f disconnect;
    rmem_malloc_f malloc;
    rmem_put_f put;
    rmem_get_f get;
    rmem_free_f free;
    rmem_atomic_commit_f atomic_commit;
    rmem_register_data_f register_data;
    rmem_deregister_data_f deregister_data;

    void* layer_data; // layer-specific data
} rmem_layer_t;

#endif // _BACKUP_INTERFACE_H_

