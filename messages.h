#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

#include <stdint.h>
#include "tag_addr_map.h"

#define MULTI_OP_MAX_ITEMS 20

enum message_id {
    MSG_INVALID = 0,
    MSG_MR,
    MSG_ALLOC,
    MSG_LOOKUP,
    MSG_MEMRESP,
    MSG_TXN_FREE,
    MSG_TXN_CP,
    MSG_TXN_GO,
    MSG_TXN_ABORT,
    MSG_TXN_ACK,
    MSG_TAG_ADDR_MAP,
    MSG_STARTUP_ACK,
    MSG_MULTI_ALLOC,
    MSG_MULTI_LOOKUP,
    MSG_MULTI_MEMRESP,
    MSG_MULTI_TXN_FREE,
    MSG_MULTI_TXN_CP
};

struct message {
    enum message_id id;

    union {
        struct {
            uint64_t addr;
            uint32_t rkey;
        } mr;
        struct {
            uint64_t size;
            uint32_t tag;
        } alloc;
        struct {
            uint64_t addr;
        } free;
        struct {
            uint32_t tag;
        } lookup;
        struct {
            uint64_t addr;
            int8_t error;
        } memresp;
	struct {
	    uint64_t dst;
	    uint64_t src;
	    uint64_t size;
	} cp;
	struct {
            int size;
            int nleft;
	    tag_addr_entry_t data[TAG_ADDR_MAP_SIZE_MSG];
	} tag_addr_map;
	struct {
	    uint64_t size;
	    uint32_t tags[MULTI_OP_MAX_ITEMS];
	    uint8_t nitems;
	} multi_alloc;
	struct {
	    uint32_t tags[MULTI_OP_MAX_ITEMS];
	    uint8_t nitems;
	} multi_lookup;
	struct {
	    uint64_t addrs[MULTI_OP_MAX_ITEMS];
	    int8_t error;
	    uint8_t nitems;
	} multi_memresp;
	struct {
	    uint64_t addrs[MULTI_OP_MAX_ITEMS];
	    uint8_t nitems;
	} multi_free;
	struct {
	    uint64_t dsts[MULTI_OP_MAX_ITEMS];
	    uint64_t srcs[MULTI_OP_MAX_ITEMS];
	    uint64_t sizes[MULTI_OP_MAX_ITEMS];
	    uint8_t nitems;
	} multi_cp;
    } data;
};

#endif
