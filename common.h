#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

#define rvm_log(M, ... ) fprintf(stderr, "RVM_LOG %s:%d: " M, \
    __FILE__, __LINE__, ##__VA_ARGS__)

#define TEST_NZ(x) do { if ( (x)) rc_die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) rc_die("error: " #x " failed (returned zero/null)."); } while (0)

/* Generic bit-array data stucture. Taken from:
 * http://c-faq.com/misc/bitsets.html"
 */
typedef int32_t bitmap_t;
/* Internal (don't use) */
#define _BITMASK(b) (1 << ((b) % 32))
/* Internal (don't use) */
#define _BITSLOT(b) ((b) / 32)
/* Set a bit in the map */
#define BITSET(a, b) ((a)[_BITSLOT(b)] |= _BITMASK(b))
/* Clear a bit in the map */
#define BITCLEAR(a, b) ((a)[_BITSLOT(b)] &= ~_BITMASK(b))
/* Test if a bit is set */
#define BITTEST(a, b) ((a)[_BITSLOT(b)] & _BITMASK(b))
/* How man slots of type bitmap_t you must alloc
 * (bitmap_t map[BITNSLOTS(number_of_bits)]) */
#define BITNSLOTS(nb) ((nb + 32 - 1) / 32)

typedef void (*pre_conn_cb_fn)(struct rdma_cm_id *id);
typedef void (*connect_cb_fn)(struct rdma_cm_id *id);
typedef void (*completion_cb_fn)(struct ibv_wc *wc);
typedef void (*disconnect_cb_fn)(struct rdma_cm_id *id);

void rc_init(pre_conn_cb_fn, connect_cb_fn, completion_cb_fn, disconnect_cb_fn);
void rc_client_loop(const char *host, const char *port, void *context);
void rc_disconnect(struct rdma_cm_id *id);
void rc_die(const char *message);
struct ibv_pd * rc_get_pd();
void rc_server_loop(const char *port);
void build_connection(struct rdma_cm_id *id);
void build_params(struct rdma_conn_param *params);

#endif
