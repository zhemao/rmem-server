/*
 * file_backend.c
 *
 *  Created on: May 8, 2015
 *      Author: violet
 */


/* Of type rmem_connect_f */
void file_connect(rmem_layer_t* rcfg, char* host, char* port)
{

}

/* Of type rmem_disconnect_f */
void file_disconnect(rmem_layer_t* rcfg)
{

}

/* Of type rmem_malloc_f */
uint64_t file_malloc(rmem_layer_t* rcfg, size_t size, uint32_t tag)
{

}

/* Of type rmem_free_f */
int file_free(rmem_layer_t* rcfg, uint32_t tag)
{

}

/* Of type rmem_put_f */
int file_put(rmem_layer_t* rcfg, uint32_t tag,
        void *src, void *src_reg, size_t size)
{

}

/* Of type rmem_get_f */
int file_get(rmem_layer_t* rcfg, void *dst,
        void *dst_reg, uint32_t tag, size_t size)
{

}

/* Of type rmem_atomic_commit_f */
int file_atomic_commit(rmem_layer_t* rcfg,
        uint32_t* tags_src, uint32_t* tags_dst, uint32_t* sizes, uint32_t ntag)
{

}

/* Of type rmem_register_data_f */
void* file_register_data(rmem_layer_t* rcfg,
        void* buf, size_t size)
{

}

/* Of type rmem_deregister_data_f */
void file_deregister_data(rmem_layer_t* rcfg, void*buf)
{

}

/* Of type create_rmem_layer_f */
rmem_layer_t* create_file_layer()
{

}
