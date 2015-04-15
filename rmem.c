#include "common.h"
#include "rmem.h"

static const int TIMEOUT_IN_MS = 500;

static int send_message(struct rdma_cm_id *id)
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

    return ibv_post_send(id->qp, &wr, &bad_wr);
}

static int post_receive(struct rdma_cm_id *id)
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

    return ibv_post_recv(id->qp, &wr, &bad_wr);
}

static void setup_memory(struct client_context *ctx)
{
    posix_memalign((void **)&ctx->recv_msg, sysconf(_SC_PAGESIZE),
            sizeof(*ctx->recv_msg));
    TEST_Z(ctx->recv_msg_mr = ibv_reg_mr(rc_get_pd(), ctx->recv_msg,
            sizeof(*ctx->recv_msg),
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    posix_memalign((void **)&ctx->send_msg, sysconf(_SC_PAGESIZE),
            sizeof(*ctx->send_msg));
    TEST_Z(ctx->send_msg_mr = ibv_reg_mr(rc_get_pd(), ctx->send_msg,
            sizeof(*ctx->send_msg), IBV_ACCESS_LOCAL_WRITE));
}

static void on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
    struct client_context *ctx = (struct client_context *)id->context;

    switch (wc->opcode) {
    case IBV_WC_RECV:
        sem_post(&ctx->recv_sem);
        break;
    case IBV_WC_RDMA_READ:
    case IBV_WC_RDMA_WRITE:
        sem_post(&ctx->rdma_sem);
        break;
    case IBV_WC_SEND:
        sem_post(&ctx->send_sem);
        break;
    default:
        break;
    }
}

void rmem_connect(struct rmem *rmem, const char *host, const char *port)
{
    struct addrinfo *addr;
    struct rdma_conn_param cm_params;
    struct rdma_cm_event *event = NULL;

    TEST_NZ(getaddrinfo(host, port, NULL, &addr));
    TEST_Z(rmem->ec = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(rmem->ec, &rmem->id, NULL, RDMA_PS_TCP));
    TEST_NZ(rdma_resolve_addr(rmem->id, NULL, addr->ai_addr, TIMEOUT_IN_MS));

    freeaddrinfo(addr);

    rmem->id->context = &rmem->ctx;
    build_params(&cm_params);
    rc_init(NULL, NULL, on_completion, NULL);

    TEST_NZ(sem_init(&rmem->ctx.rdma_sem, 0, 0));
    TEST_NZ(sem_init(&rmem->ctx.send_sem, 0, 0));
    TEST_NZ(sem_init(&rmem->ctx.recv_sem, 0, 0));

    while (rdma_get_cm_event(rmem->ec, &event) == 0) {
        struct rdma_cm_event event_copy;

        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED) {
            build_connection(event_copy.id);
            setup_memory(&rmem->ctx);
            TEST_NZ(post_receive(rmem->id));
            TEST_NZ(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));
        } else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
            TEST_NZ(rdma_connect(event_copy.id, &cm_params));
        } else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {
            break;
        }
    }

    // wait for MR recv
    sem_wait(&rmem->ctx.recv_sem);
    rmem->ctx.peer_addr = rmem->ctx.recv_msg->data.mr.addr;
    rmem->ctx.peer_rkey = rmem->ctx.recv_msg->data.mr.rkey;
}

void rmem_disconnect(struct rmem *rmem)
{
    rdma_disconnect(rmem->id);

    TEST_NZ(sem_destroy(&rmem->ctx.rdma_sem));
    TEST_NZ(sem_destroy(&rmem->ctx.send_sem));
    TEST_NZ(sem_destroy(&rmem->ctx.recv_sem));

    ibv_dereg_mr(rmem->ctx.recv_msg_mr);
    ibv_dereg_mr(rmem->ctx.send_msg_mr);

    free(rmem->ctx.recv_msg);
    free(rmem->ctx.send_msg);

    rdma_destroy_id(rmem->id);
    rdma_destroy_event_channel(rmem->ec);
}

uint64_t rmem_malloc(struct rmem *rmem, size_t size, uint32_t tag)
{
    struct client_context *ctx = &rmem->ctx;

    ctx->send_msg->id = MSG_ALLOC;
    ctx->send_msg->data.alloc.size = size;
    ctx->send_msg->data.alloc.tag = tag;


    if (post_receive(rmem->id))
        return 0;
    if (send_message(rmem->id))
        return 0;

    if (sem_wait(&ctx->send_sem))
        return 0;
    if (sem_wait(&ctx->recv_sem))
        return 0;

    if (ctx->recv_msg->data.memresp.error)
        return 0;

    return ctx->recv_msg->data.memresp.addr;
}

int rmem_put(struct rmem *rmem, uint64_t dst,
        void *src, struct ibv_mr *src_mr, size_t size)
{
    int err;
    struct client_context *ctx = &rmem->ctx;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t) rmem->id;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.imm_data = htonl(size);
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = dst;
    wr.wr.rdma.rkey = ctx->peer_rkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t) src;
    sge.length = size;
    sge.lkey = src_mr->lkey;

    if ((err = ibv_post_send(rmem->id->qp, &wr, &bad_wr)) != 0)
        return err;
    if (sem_wait(&ctx->rdma_sem))
        return errno;
    return 0;
}

int rmem_get(struct rmem *rmem, void *dst, struct ibv_mr *dst_mr,
        uint64_t src, size_t size)
{
    struct client_context *ctx = &rmem->ctx;
    int err;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t) rmem->id;
    wr.opcode = IBV_WR_RDMA_READ;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = src;
    wr.wr.rdma.rkey = ctx->peer_rkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t) dst;
    sge.length = size;
    sge.lkey = dst_mr->lkey;


    if ((err = ibv_post_send(rmem->id->qp, &wr, &bad_wr)) != 0)
        return err;
    if (sem_wait(&ctx->rdma_sem))
        return errno;
    return 0;
}

int rmem_free(struct rmem *rmem, uint64_t addr)
{
    struct client_context *ctx = &rmem->ctx;

    ctx->send_msg->id = MSG_FREE;
    ctx->send_msg->data.free.addr = addr;

    if (send_message(rmem->id))
        return -1;
    if (sem_wait(&ctx->send_sem))
        return -1;

    return 0;
}

struct ibv_mr *rmem_create_mr(void *data, size_t size)
{
    return ibv_reg_mr(rc_get_pd(), data,
            size, IBV_ACCESS_LOCAL_WRITE);
}
