#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>

#include "common.h"
#include "messages.h"
#include "rmem_table.h"
#include "rmem.h"
#include "utils/log.h"
#include "utils/error.h"
#include "utils/stats.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char *DEFAULT_PORT = "12345";
static const size_t BUFFER_SIZE = 10 * 1024 * 1024;

struct stats stats;
struct rmem_table rmem;
pthread_mutex_t alloc_mutex;

struct conn_context
{
    struct ibv_mr *rmem_mr;

    struct message *recv_msg;
    struct ibv_mr *recv_msg_mr;

    struct message *send_msg;
    struct ibv_mr *send_msg_mr;

    struct rmem_txn_list txn_list;

    sem_t ack_sem;
};

static
void insert_tag_to_addr(struct rmem_table* rmem, uint32_t tag, uintptr_t addr) {
    uintptr_t* value = (uintptr_t*)malloc(sizeof(uintptr_t));
    CHECK_ERROR(value == 0,
            ("Failure: error allocating value"));
    *value = addr;
    hash_insert_item(rmem->tag_to_addr, tag, value);
}

static void send_message(struct rdma_cm_id *id)
{
    stats_start(KSTATS_SEND_MSG);

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

    LOG(1, ("posting sending WR\n"));

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
    
    stats_end(KSTATS_SEND_MSG);
}

static void post_msg_receive(struct rdma_cm_id *id)
{
    stats_start(KSTATS_POST_MSG_RECV);

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

    LOG(1, ("posting receive WR\n"));

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
    
    stats_end(KSTATS_POST_MSG_RECV);
}

static void on_pre_conn(struct rdma_cm_id *id)
{
    stats_start(KSTATS_PRE_CONN);

    struct conn_context *ctx = (struct conn_context *) malloc(
            sizeof(struct conn_context));

    id->context = ctx;

    LOG(5, ("Creating rmem mr. addr: %ld size: %d\n", 
                (uintptr_t)rmem.mem, RMEM_SIZE));
    TEST_Z(ctx->rmem_mr = ibv_reg_mr(
                rc_get_pd(), rmem.mem, RMEM_SIZE,
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                IBV_ACCESS_REMOTE_READ));

    TEST_NZ(posix_memalign((void **)&ctx->recv_msg, sysconf(_SC_PAGESIZE),
            sizeof(*ctx->recv_msg)));
    TEST_Z(ctx->recv_msg_mr = ibv_reg_mr(
                rc_get_pd(), ctx->recv_msg, sizeof(*ctx->recv_msg),
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

    TEST_NZ(posix_memalign((void **)&ctx->send_msg, sysconf(_SC_PAGESIZE),
            sizeof(*ctx->send_msg)));
    TEST_Z(ctx->send_msg_mr = ibv_reg_mr(
                rc_get_pd(), ctx->send_msg, sizeof(*ctx->send_msg),
                IBV_ACCESS_LOCAL_WRITE));

    txn_list_init(&ctx->txn_list);

    TEST_NZ(sem_init(&ctx->ack_sem, 0, 0));

    stats_end(KSTATS_PRE_CONN);
}

static void send_tag_to_addr_info(struct rdma_cm_id *id) 
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    int hash_n = hash_num_elements(rmem.tag_to_addr);
    int map_size_left = hash_n;

    hash_iterator_t it = hash_begin(rmem.tag_to_addr);

    assert( (map_size_left == 0) == (hash_is_iterator_null(it)) );

    ctx->send_msg->id = MSG_TAG_ADDR_MAP;

    if (map_size_left == 0) {
	ctx->send_msg->data.tag_addr_map.size = 0;
	send_message(id);
	return;
    }

    while (map_size_left > 0) {
	int to_send = MIN(map_size_left, TAG_ADDR_MAP_SIZE_MSG);
	ctx->send_msg->data.tag_addr_map.size = to_send;

	printf("Sending %d mappings\n", to_send);

	int i = 0;
	for (; i < to_send; ++i) {
	    assert(!hash_is_iterator_null(it));

	    ctx->send_msg->data.tag_addr_map.data[i].tag = 
		(uint32_t)hash_iterator_key(it);

	    uintptr_t addr = *(uintptr_t*)hash_iterator_value(it);
	    ctx->send_msg->data.tag_addr_map.data[i].addr = addr;

	    hash_next_iterator(it);
	}

	send_message(id);
	TEST_NZ(sem_wait(&ctx->ack_sem));
	map_size_left -= to_send;
    }
}

static void on_connection(struct rdma_cm_id *id)
{
    stats_start(KSTATS_ON_CONN);

    struct conn_context *ctx = (struct conn_context *)id->context;

    // post the initial receive
    post_msg_receive(id);

    ctx->send_msg->id = MSG_MR;
    ctx->send_msg->data.mr.addr = (uintptr_t)ctx->rmem_mr->addr;
    ctx->send_msg->data.mr.rkey = ctx->rmem_mr->rkey;

    send_message(id);
    TEST_NZ(sem_wait(&ctx->ack_sem));

    LOG(5, ("On connection. mr.addrd: %ld rmem.mem: %ld\n", (uintptr_t)ctx->send_msg->data.mr.addr,
                (uintptr_t)rmem.mem));

    send_tag_to_addr_info(id);

    stats_end(KSTATS_ON_CONN);
}

static void on_completion(struct ibv_wc *wc)
{
    //stats_start(KSTATS_ON_COMPL);

    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
    struct conn_context *ctx = (struct conn_context *)id->context;

    if (wc->opcode == IBV_WC_RECV) {
        struct message *msg = ctx->recv_msg;
        void *ptr;
        LOG(1, ("on_completion: IBV_WC_RECV\n"));

        switch (msg->id) {
            case MSG_ALLOC:
                TEST_NZ(pthread_mutex_lock(&alloc_mutex));
                ptr = rmem_table_alloc(&rmem, msg->data.alloc.size,
                        msg->data.alloc.tag);
                TEST_NZ(pthread_mutex_unlock(&alloc_mutex));
                ctx->send_msg->id = MSG_MEMRESP;
                ctx->send_msg->data.memresp.addr = (uintptr_t) ptr;
                ctx->send_msg->data.memresp.error = (ptr == NULL);

                insert_tag_to_addr(&rmem, msg->data.alloc.tag, (uintptr_t)ptr);
                
                LOG(1, ("MSG_ALLOC size: %ld ptr: %ld\n", 
                            msg->data.alloc.size, (uintptr_t)ptr));

#ifdef DEBUG
                if (ptr == NULL) {
                    printf("Error allocating\n");
                }
#endif

                send_message(id);
                break;
            case MSG_LOOKUP:
                LOG(1, ("MSG_LOOKUP\n"));
                ptr = rmem_table_lookup(&rmem, msg->data.lookup.tag);
                ctx->send_msg->id = MSG_MEMRESP;
                ctx->send_msg->data.memresp.addr = (uintptr_t) ptr;
                ctx->send_msg->data.memresp.error = (ptr == NULL);
                send_message(id);
                break;
            case MSG_TXN_FREE:
                LOG(1, ("MSG_TXN_FREE\n"));
		txn_list_add_free(&ctx->txn_list,
			(void *) msg->data.free.addr);
                ctx->send_msg->id = MSG_TXN_ACK;
                send_message(id);
                break;
            case MSG_TXN_CP:
                LOG(1, ("MSG_TXN_CP\n"));
                txn_list_add_cp(&ctx->txn_list,
                        (void *) msg->data.cp.dst,
                        (void *) msg->data.cp.src,
                        (size_t) msg->data.cp.size);
                ctx->send_msg->id = MSG_TXN_ACK;
                send_message(id);
                break;
            case MSG_TXN_GO:
                LOG(1, ("MSG_TXN_GO\n"));
		txn_commit(&rmem, &ctx->txn_list);
                ctx->send_msg->id = MSG_TXN_ACK;
                send_message(id);
                break;
            case MSG_TXN_ABORT:
                LOG(1, ("MSG_TXN_ABORT\n"));
		txn_list_clear(&ctx->txn_list);
                ctx->send_msg->id = MSG_TXN_ACK;
                send_message(id);
                break;
	    case MSG_STARTUP_ACK:
		TEST_NZ(sem_post(&ctx->ack_sem));
		break;
            default:
                fprintf(stderr, "Invalid message type %d\n", msg->id);
                exit(EXIT_FAILURE);
        }

        post_msg_receive(id);
    } else {
        LOG(1, ("on_completion: else\n"));
    }
    
    //stats_end(KSTATS_ON_COMPL);
}

static void on_disconnect(struct rdma_cm_id *id)
{
    struct conn_context *ctx = (struct conn_context *)id->context;

    LOG(1, ("on_disconnect\n"));

    txn_list_clear(&ctx->txn_list);

    ibv_dereg_mr(ctx->rmem_mr);
    ibv_dereg_mr(ctx->recv_msg_mr);
    ibv_dereg_mr(ctx->send_msg_mr);

    free(ctx->send_msg);
    free(ctx->recv_msg);

    free(ctx);
}

void ctrlc_handler(int sig_num) 
{
    stats_dump();
}

void set_ctrlc_handler()
{
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = ctrlc_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction(SIGINT, &sig_int_handler, NULL);
}

int main(int argc, char **argv)
{
    const char *port;

    if (argc > 1)
	port = argv[1];
    else
	port = DEFAULT_PORT;

    LOG(1, ("starting rmem-server\n"));
    init_rmem_table(&rmem);
    pthread_mutex_init(&alloc_mutex, NULL);

    stats_init();
    set_ctrlc_handler();

    rc_init(
            on_pre_conn,
            on_connection,
            on_completion,
            on_disconnect);

    printf("waiting for connections. interrupt (^C) to exit.\n");

    rc_server_loop(port);

    free_rmem_table(&rmem);
    pthread_mutex_destroy(&alloc_mutex);

    return 0;
}

