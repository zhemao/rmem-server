#ifndef __RMEM_H__
#define __RMEM_H__

#include "messages.h"

#include <rdma/rdma_cma.h>
#include <semaphore.h>

struct client_context {
	char *buffer;
	struct ibv_mr *buffer_mr;

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

void rmem_connect(struct rmem *rmem, const char *host, const char *port, size_t size);
void rmem_disconnect(struct rmem *rmem);
uint64_t rmem_malloc(struct rmem *rmem, size_t size, uint32_t tag);
int rmem_put(struct rmem *rmem, uint64_t dst, void *src, size_t size);
int rmem_get(struct rmem *rmem, void *dst, uint64_t src, size_t size);
int rmem_free(struct rmem *rmem, uint64_t addr);

#endif
