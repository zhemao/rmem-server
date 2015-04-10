#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

#include <stdint.h>

enum message_id {
	MSG_INVALID = 0,
	MSG_MR,
	MSG_ALLOC,
	MSG_LOOKUP,
	MSG_FREE,
	MSG_MEMRESP,
	MSG_WACK
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
	} data;
};

#endif
