#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rmem.h"

static const char *DEFAULT_PORT = "12345";

int main(int argc, char **argv)
{
	struct rmem rmem;
	uint64_t size;
	uint32_t tag;
	uint64_t raddr;
	char data[2048];
	int i;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <server-address> <size> <tag>\n", argv[0]);
		return 1;
	}

	size = atoi(argv[2]);
	tag = atoi(argv[3]);

	if (size >= 1024) {
		fprintf(stderr, "Size must be smaller than 1024\n");
		return -1;
	}

	printf("Connecting to server...\n");
	rmem_connect(&rmem, argv[1], DEFAULT_PORT, size + 1);

	printf("Allocating memory...\n");
	raddr = rmem_malloc(&rmem, size, tag);
	if (raddr == 0) {
		fprintf(stderr, "Failed to allocate remote memory\n");
		exit(EXIT_FAILURE);
	}
	printf("Remote memory has addr %lx\n", raddr);

	memset(data, 0, sizeof(data));

	for (i = 0; i < size; i++)
		data[i] = random();

	printf("Writing random data...\n");
	rmem_put(&rmem, raddr, data, size);
	printf("Reading it back...\n");
	rmem_get(&rmem, data + size, raddr, size);

	if (memcmp(data, data + size, size) == 0)
		printf("Data matches\n");

	printf("Freeing remote memory...\n");
	if (rmem_free(&rmem, raddr))
		fprintf(stderr, "Failed to free remote memory\n");

	rmem_disconnect(&rmem);

	return 0;
}

