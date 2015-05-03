#ifndef __RMEM_H__
#define __RMEM_H__

#include "rmem_generic_interface.h"
#include "messages.h"

#include <rdma/rdma_cma.h>
#include <semaphore.h>
#include "data/hash.h"

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
    hash_t tag_to_addr;
    
//    struct ibv_mr *blk_tbl_mr;   /**< IB registration info for block table */
};

rmem_layer_t* create_rmem_layer();

#endif

