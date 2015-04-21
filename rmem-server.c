#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"
#include "messages.h"
#include "rmem_table.h"

static const char *DEFAULT_PORT = "12345";
static const size_t BUFFER_SIZE = 10 * 1024 * 1024;

struct rmem_table rmem;
pthread_mutex_t alloc_mutex;

struct conn_context
{
    struct ibv_mr *rmem_mr;

    struct message *recv_msg;
    struct ibv_mr *recv_msg_mr;

    struct message *send_msg;
    struct ibv_mr *send_msg_mr;

    struct rmem_cp_info_list cp_info;
};

static void send_message(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

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

static void post_msg_receive(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t) ctx->recv_msg;
    sge.length = sizeof(*ctx->recv_msg);
    sge.lkey = ctx->recv_msg_mr->lkey;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

static void on_pre_conn(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *) malloc(
		    sizeof(struct conn_context));

    id->context = ctx;

    TEST_Z(ctx->rmem_mr = ibv_reg_mr(
            rc_get_pd(), rmem.mem, RMEM_SIZE,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
            IBV_ACCESS_REMOTE_READ));

    posix_memalign((void **)&ctx->recv_msg, sysconf(_SC_PAGESIZE),
		    sizeof(*ctx->recv_msg));
    TEST_Z(ctx->recv_msg_mr = ibv_reg_mr(
            rc_get_pd(), ctx->recv_msg, sizeof(*ctx->recv_msg),
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    posix_memalign((void **)&ctx->send_msg, sysconf(_SC_PAGESIZE),
		    sizeof(*ctx->send_msg));
    TEST_Z(ctx->send_msg_mr = ibv_reg_mr(
            rc_get_pd(), ctx->send_msg, sizeof(*ctx->send_msg),
            IBV_ACCESS_LOCAL_WRITE));

    cp_info_list_init(&ctx->cp_info);

    post_msg_receive(id);
}

static void on_connection(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    ctx->send_msg->id = MSG_MR;
    ctx->send_msg->data.mr.addr = (uintptr_t)ctx->rmem_mr->addr;
    ctx->send_msg->data.mr.rkey = ctx->rmem_mr->rkey;

    send_message(id);
}

static void on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
    struct conn_context *ctx = (struct conn_context *)id->context;

    if (wc->opcode == IBV_WC_RECV) {
        struct message *msg = ctx->recv_msg;
        void *ptr;

        switch (msg->id) {
        case MSG_ALLOC:
            TEST_NZ(pthread_mutex_lock(&alloc_mutex));
            ptr = rmem_alloc(&rmem, msg->data.alloc.size,
                    msg->data.alloc.tag);
            TEST_NZ(pthread_mutex_unlock(&alloc_mutex));
            ctx->send_msg->id = MSG_MEMRESP;
            ctx->send_msg->data.memresp.addr = (uintptr_t) ptr;
            ctx->send_msg->data.memresp.error = (ptr == NULL);
            send_message(id);
            break;
        case MSG_LOOKUP:
            ptr = rmem_table_lookup(&rmem, msg->data.lookup.tag);
            ctx->send_msg->id = MSG_MEMRESP;
            ctx->send_msg->data.memresp.addr = (uintptr_t) ptr;
            ctx->send_msg->data.memresp.error = (ptr == NULL);
            send_message(id);
            break;
        case MSG_FREE:
            ptr = (void *) msg->data.free.addr;
            rmem_free(&rmem, ptr);
            break;
	case MSG_CP_REQ:
	    cp_info_list_add(&ctx->cp_info,
		    (void *) msg->data.cpreq.dst,
		    (void *) msg->data.cpreq.src,
		    (size_t) msg->data.cpreq.size);
	    break;
	case MSG_CP_GO:
	    multi_cp(&ctx->cp_info);
	    cp_info_list_clear(&ctx->cp_info);
	    ctx->send_msg->id = MSG_CP_ACK;
	    send_message(id);
	    break;
	case MSG_CP_ABORT:
	    cp_info_list_clear(&ctx->cp_info);
	    break;
        default:
            fprintf(stderr, "Invalid message type %d\n", msg->id);
            exit(EXIT_FAILURE);
        }

	post_msg_receive(id);
    }
}

static void on_disconnect(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    cp_info_list_clear(&ctx->cp_info);

    ibv_dereg_mr(ctx->rmem_mr);
    ibv_dereg_mr(ctx->recv_msg_mr);
    ibv_dereg_mr(ctx->send_msg_mr);

    free(ctx->send_msg);
    free(ctx->recv_msg);

    free(ctx);
}

int main(int argc, char **argv)
{
    init_rmem_table(&rmem);
    pthread_mutex_init(&alloc_mutex, NULL);

    rc_init(
        on_pre_conn,
        on_connection,
        on_completion,
        on_disconnect);

    printf("waiting for connections. interrupt (^C) to exit.\n");

    rc_server_loop(DEFAULT_PORT);

    free_rmem_table(&rmem);
    pthread_mutex_destroy(&alloc_mutex);

    return 0;
}
