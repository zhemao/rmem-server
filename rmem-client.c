#include <fcntl.h>
#include <libgen.h>

#include "common.h"
#include "messages.h"

struct client_context
{
	char *buffer;
	char *ref;
	struct ibv_mr *buffer_mr;

	struct message *send_msg;
	struct ibv_mr *send_msg_mr;

	struct message *recv_msg;
	struct ibv_mr *recv_msg_mr;

	uint64_t peer_addr;
	uint32_t peer_rkey;

	uint64_t offset;
	uint64_t size;
	uint32_t tag;
};

static void write_remote(struct rdma_cm_id *id, uint64_t offset, uint32_t len)
{
	struct client_context *ctx = (struct client_context *)id->context;

	struct ibv_send_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = (uintptr_t)id;
	wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
	wr.imm_data = htonl(offset);
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = ctx->peer_addr + offset;
	wr.wr.rdma.rkey = ctx->peer_rkey;

	if (len) {
		wr.sg_list = &sge;
		wr.num_sge = 1;

		sge.addr = (uintptr_t)ctx->buffer;
		sge.length = len;
		sge.lkey = ctx->buffer_mr->lkey;
	}

	TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void read_remote(struct rdma_cm_id *id, uint64_t offset, uint32_t len)
{
	struct client_context *ctx = (struct client_context *)id->context;

	struct ibv_send_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = (uintptr_t)id;
	wr.opcode = IBV_WR_RDMA_READ;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = ctx->peer_addr + offset;
	wr.wr.rdma.rkey = ctx->peer_rkey;

	if (len) {
		wr.sg_list = &sge;
		wr.num_sge = 1;

		sge.addr = (uintptr_t)ctx->buffer;
		sge.length = len;
		sge.lkey = ctx->buffer_mr->lkey;
	}

	TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void send_message(struct rdma_cm_id *id)
{
	struct client_context *ctx = (struct client_context *)id->context;

	struct ibv_send_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = (uintptr_t)id;
	wr.opcode = IBV_WR_SEND;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.send_flags = IBV_SEND_SIGNALED;

	sge.addr = (uintptr_t)ctx->send_msg;
	sge.length = sizeof(*ctx->send_msg);
	sge.lkey = ctx->send_msg_mr->lkey;

	TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void post_receive(struct rdma_cm_id *id)
{
	struct client_context *ctx = (struct client_context *)id->context;

	struct ibv_recv_wr wr, *bad_wr = NULL;
	struct ibv_sge sge;

	memset(&wr, 0, sizeof(wr));

	wr.wr_id = (uintptr_t)id;
	wr.sg_list = &sge;
	wr.num_sge = 1;

	sge.addr = (uintptr_t)ctx->recv_msg;
	sge.length = sizeof(*ctx->recv_msg);
	sge.lkey = ctx->recv_msg_mr->lkey;

	TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

static void send_malloc_req(struct rdma_cm_id *id, uint64_t size, uint32_t tag)
{
	struct client_context *ctx = (struct client_context *)id->context;

	ctx->send_msg->id = MSG_ALLOC;
	ctx->send_msg->data.alloc.size = size;
	ctx->send_msg->data.alloc.tag = tag;
	send_message(id);
	post_receive(id);
}

static void send_free_req(struct rdma_cm_id *id, uint64_t offset)
{
	struct client_context *ctx = (struct client_context *)id->context;

	ctx->send_msg->id = MSG_FREE;
	ctx->send_msg->data.free.offset = offset;
	send_message(id);
}

static void on_pre_conn(struct rdma_cm_id *id)
{
	struct client_context *ctx = (struct client_context *)id->context;

	TEST_Z(ctx->ref = malloc(ctx->size));
	posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), ctx->size);
	TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(), ctx->buffer,
			ctx->size, IBV_ACCESS_LOCAL_WRITE));

	posix_memalign((void **)&ctx->recv_msg, sysconf(_SC_PAGESIZE),
			sizeof(*ctx->recv_msg));
	TEST_Z(ctx->recv_msg_mr = ibv_reg_mr(rc_get_pd(), ctx->recv_msg,
			sizeof(*ctx->recv_msg),
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

	posix_memalign((void **)&ctx->send_msg, sysconf(_SC_PAGESIZE),
			sizeof(*ctx->send_msg));
	TEST_Z(ctx->send_msg_mr = ibv_reg_mr(rc_get_pd(), ctx->send_msg,
			sizeof(*ctx->send_msg), IBV_ACCESS_LOCAL_WRITE));

	post_receive(id);
}

static void write_random_data(struct rdma_cm_id *id, uint64_t offset, uint32_t len)
{
	struct client_context *ctx = (struct client_context *)id->context;
	int i;
	long word;
	int word_size = sizeof(long);
	int chunk_size;

	for (i = 0; i < len; i += word_size) {
		word = random();
		chunk_size = ((len - i) < word_size) ? (len - i) : word_size;
		memcpy(ctx->ref + i, &word, chunk_size);
	}

	memcpy(ctx->buffer, ctx->ref, len);

	write_remote(id, offset, len);
}

static void on_completion(struct ibv_wc *wc)
{
	struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
	struct client_context *ctx = (struct client_context *)id->context;

	if (wc->opcode & IBV_WC_RECV) {
		if (ctx->recv_msg->id == MSG_MR) {
			ctx->peer_addr = ctx->recv_msg->data.mr.addr;
			ctx->peer_rkey = ctx->recv_msg->data.mr.rkey;

			printf("received MR (%lu, %u), allocating data\n",
					ctx->peer_addr, ctx->peer_rkey);
			send_malloc_req(id, ctx->size, ctx->tag);
		} else if (ctx->recv_msg->id == MSG_MEMRESP) {
			if (!ctx->recv_msg->data.memresp.error) {
				ctx->offset = ctx->recv_msg->data.memresp.offset;
				printf("Allocation finished with offset 0x%lx. "
					"Writing random data.\n", ctx->offset);
				write_random_data(id, ctx->offset, ctx->size);
			} else {
				fprintf(stderr, "Error in memory allocation.\n");
				rc_disconnect(id);
			}
		}

	} else if (wc->opcode == IBV_WC_RDMA_WRITE) {
		printf("Wrote data, now reading it back\n");
		read_remote(id, ctx->offset, ctx->size);
	} else if (wc->opcode == IBV_WC_RDMA_READ) {
		if (memcmp(ctx->buffer, ctx->ref, ctx->size) != 0)
			printf("Data read back does not match.\n");
		else
			printf("Data read back matches.\n");
		send_free_req(id, ctx->offset);
	} else if (wc->opcode == IBV_WC_SEND) {
		if (ctx->send_msg->id == MSG_FREE) {
			printf("Memory freed. Disconnecting...\n");
			rc_disconnect(id);
		}
	}
}

int main(int argc, char **argv)
{
	struct client_context ctx;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <server-address> <size> <tag>\n", argv[0]);
		return 1;
	}

	ctx.size = atoi(argv[2]);
	ctx.tag = atoi(argv[3]);

	rc_init(
		on_pre_conn,
		NULL, // on connect
		on_completion,
		NULL); // on disconnect

	rc_client_loop(argv[1], DEFAULT_PORT, &ctx);

	return 0;
}

