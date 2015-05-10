/*
 * stub_backend.c
 *
 *  Created on: May 8, 2015
 *      Author: violet
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stub_backend.h>
#include "error.h"

/* Of type rmem_connect_f */
void stub_connect(rmem_layer_t* rcfg, char* host, char* port)
{
    return;
}

/* Of type rmem_disconnect_f */
void stub_disconnect(rmem_layer_t* rcfg)
{
    return;
}

/* Of type rmem_malloc_f */
uint64_t stub_malloc(rmem_layer_t* rcfg, size_t size, uint32_t tag)
{
    return STUB_BACKEND_MAGIC;
}

/* Of type rmem_free_f */
int stub_free(rmem_layer_t* rcfg, uint32_t tag)
{
    return 0;
}

int stub_multi_malloc(rmem_layer_t *rcfg,
    uint64_t *addrs, uint64_t size, uint32_t *tags, uint32_t n)
{
    for(int i = 0; i < n; i++)
    {
        addrs[i] = STUB_BACKEND_MAGIC;
    }

    return 0;
}

int stub_multi_free(rmem_layer_t *rcfg,
    uint32_t *tags, uint32_t n)
{
    return 0;
}

/* Of type rmem_put_f */
int stub_put(rmem_layer_t* rcfg, uint32_t tag,
        void *src, void *src_reg, size_t size)
{
    return 0;
}

/* Of type rmem_get_f */
int stub_get(rmem_layer_t* rcfg, void *dst,
        void *dst_reg, uint32_t tag, size_t size)
{
    UNIMPLEMENTED;

    return -1;
}

/* Of type rmem_atomic_commit_f */
int stub_atomic_commit(rmem_layer_t* rcfg,
        uint32_t* tags_src, uint32_t* tags_dst, uint32_t* sizes, uint32_t ntag)
{
    return 0;
}

/* Of type rmem_register_data_f */
void* stub_register_data(rmem_layer_t* rcfg,
        void* buf, size_t size)
{
    return (void*)STUB_BACKEND_MAGIC;
}

/* Of type rmem_deregister_data_f */
void stub_deregister_data(rmem_layer_t* rcfg, void*buf)
{
    return;
}

/* Of type create_rmem_layer_f */
rmem_layer_t* create_stub_layer()
{
    rmem_layer_t *rcfg = malloc(sizeof(rmem_layer_t));
    CHECK_ERROR(rcfg == 0, ("Failed to allocate stub layer\n"));

    /* Register callbacks */
    rcfg->connect = stub_connect;
    rcfg->disconnect = stub_disconnect;
    rcfg->malloc = stub_malloc;
    rcfg->free = stub_free;
    rcfg->multi_malloc = stub_multi_malloc;
    rcfg->multi_free = stub_multi_free;
    rcfg->put = stub_put;
    rcfg->get = stub_get;
    rcfg->atomic_commit = stub_atomic_commit;
    rcfg->register_data = stub_register_data;
    rcfg->deregister_data = stub_deregister_data;

    /* Set up local data */
    rcfg->layer_data = NULL;

    return rcfg;
}
