void rmem_connect(rmem_layer_t*, char*, char*);
void rmem_disconnect(rmem_layer_t*);

uint64_t rmem_malloc(rmem_layer_t*, size_t size, uint32_t tag);
int rmem_free(rmem_layer_t *rmem_layer, uint32_t tag);

int rmem_put(rmem_layer_t*, uint32_t tag, void *src, void *src_mr, size_t size);
int rmem_get(rmem_layer_t*, void *dst, void *dst_mr, uint32_t tag, size_t size);

int rmem_atomic_commit(rmem_layer_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t);
static void *rmem_register_data(rmem_layer_t*, void *data, size_t size);
static void rmem_deregister_data(rmem_layer_t*, void *data);

int rmem_multi_malloc(rmem_layer_t *rmem_layer, uint64_t *addrs,
	uint64_t *sizes, uint32_t *tags, uint32_t n);
int rmem_multi_free(rmem_layer_t *rmem_layer, uint32_t *tags, uint32_t n);
