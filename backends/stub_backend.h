/*
 * stub_backend.h
 *
 *  Stub backend that doesn't support recovery. Basically it always returns
 *  successfully. So long as you don't attempt any "gets" everything will work.
 *  It's intended to help you test new rvm ports to see if your normal-case
 *  execution still works properly and also to get performance baselines.
 *
 *  Created on: May 8, 2015
 *      Author: Nathan Pemberton
 */

#ifndef STUB_BACKEND_H_
#define STUB_BACKEND_H_

#include "rmem_generic_interface.h"

#define STUB_BACKEND_MAGIC 0x57BBC4E3d

/* Of type rmem_connect_f */
void stub_connect(rmem_layer_t* rcfg, char* host, char* port);

/* Of type rmem_disconnect_f */
void stub_disconnect(rmem_layer_t* rcfg);

/* Of type rmem_malloc_f */
uint64_t stub_malloc(rmem_layer_t* rcfg, size_t size, uint32_t tag);

/* Of type rmem_free_f */
int stub_free(rmem_layer_t* rcfg, uint32_t tag);

/* Of type rmem_put_f */
int stub_put(rmem_layer_t* rcfg, uint32_t tag,
        void *src, void *src_reg, size_t size);

/* Of type rmem_get_f */
int stub_get(rmem_layer_t* rcfg, void *dst,
        void *dst_reg, uint32_t tag, size_t size);

/* Of type rmem_atomic_commit_f */
int stub_atomic_commit(rmem_layer_t* rcfg,
        uint32_t* tags_src, uint32_t* tags_dst, uint32_t* sizes, uint32_t ntag);

/* Of type rmem_register_data_f */
void* stub_register_data(rmem_layer_t* rcfg,
        void* buf, size_t size);

/* Of type rmem_deregister_data_f */
void stub_deregister_data(rmem_layer_t* rcfg, void*buf);

/* Of type create_rmem_layer_f */
rmem_layer_t* create_stub_layer();

#endif /* STUB_BACKEND_H_ */
