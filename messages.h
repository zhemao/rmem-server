#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

const char *DEFAULT_PORT = "12345";
const size_t BUFFER_SIZE = 10 * 1024 * 1024;

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
			uint64_t offset;
		} free;
		struct {
			uint32_t tag;
		} lookup;
		struct {
			uint64_t offset;
			int8_t error;
		} memresp;
	} data;
};

#endif
