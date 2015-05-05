#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

#include <stdint.h>

#define TAG_ADDR_PAIR_SIZEOF sizeof(tag_addr_entry)
#define TAG_ADDR_MAP_SIZE_MSG 20


enum message_id {
    MSG_INVALID = 0,
    MSG_MR,
    MSG_ALLOC,
    MSG_LOOKUP,
    MSG_FREE,
    MSG_MEMRESP,
    MSG_CP_REQ,
    MSG_CP_GO,
    MSG_CP_ABORT,
    MSG_CP_ACK,
    MSG_TAG_ADDR_MAP,
    MSG_STARTUP_ACK
};

typedef struct tag_addr_entry {
    uint32_t tag;
    uintptr_t addr;
} tag_addr_entry_t;

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
	} cpreq;
	struct {
            int size;
	    tag_addr_entry_t data[TAG_ADDR_MAP_SIZE_MSG];
	} tag_addr_map;
    } data;
};

#endif
