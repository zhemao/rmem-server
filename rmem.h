#ifndef __RMEM_H__
#define __RMEM_H__

#include "messages.h"

#include <rdma/rdma_cma.h>
#include <semaphore.h>

struct client_context {
    struct message *send_msg;
    struct ibv_mr *send_msg_mr;

    struct message *recv_msg;
    struct ibv_mr *recv_msg_mr;

    uint64_t peer_addr;
    uint32_t peer_rkey;

    sem_t rdma_sem;
    sem_t send_sem;
    sem_t recv_sem;
};

struct rmem {
    struct rdma_cm_id *id;
    struct rdma_event_channel *ec;
    struct client_context ctx;
};

void rmem_connect(struct rmem *rmem, const char *host, const char *port);
void rmem_disconnect(struct rmem *rmem);
uint64_t rmem_malloc(struct rmem *rmem, size_t size, uint32_t tag);
uint64_t rmem_lookup(struct rmem *rmem, uint32_t tag);
int rmem_put(struct rmem *rmem, uint64_t dst, void *src, struct ibv_mr *src_mr, size_t size);
int rmem_get(struct rmem *rmem, void *dst, struct ibv_mr *dst_mr, uint64_t src, size_t size);
int rmem_free(struct rmem *rmem, uint64_t addr);
struct ibv_mr *rmem_create_mr(void *data, size_t size);
int rmem_multi_cp_add(struct rmem *rmem, uint64_t dst, uint64_t src, uint64_t size);
int rmem_multi_cp_go(struct rmem *rmem);

#endif
