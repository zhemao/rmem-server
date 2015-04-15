#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rmem.h"

static const char *DEFAULT_PORT = "12345";

int main(int argc, char **argv)
{
    struct rmem rmem;
    uint64_t size;
    uint32_t tag;
    uint64_t raddr;
    char *data;
    struct ibv_mr *data_mr;
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

    posix_memalign((void **) &data, sysconf(_SC_PAGESIZE), 2 * size);

    printf("Connecting to server...\n");
    rmem_connect(&rmem, argv[1], DEFAULT_PORT);

    data_mr = rmem_create_mr(data, 2 * size);

    printf("Allocating memory...\n");
    raddr = rmem_malloc(&rmem, size, tag);
    if (raddr == 0) {
        fprintf(stderr, "Failed to allocate remote memory\n");
        exit(EXIT_FAILURE);
    }
    printf("Remote memory has addr %lx\n", raddr);

    memset(data, 0, size);

    for (i = 0; i < size; i += sizeof(long)) {
        long word = random();
        int wsz = (size - i < sizeof(word)) ? size - i : sizeof(word);
        memcpy(data + i, &word, wsz);
    }

    printf("Writing random data...\n");
    rmem_put(&rmem, raddr, data, data_mr, size);
    printf("Reading it back...\n");
    rmem_get(&rmem, data + size, data_mr, raddr, size);

    if (memcmp(data, data + size, size) == 0)
        printf("Data matches\n");

    printf("Freeing remote memory...\n");
    if (rmem_free(&rmem, raddr))
        fprintf(stderr, "Failed to free remote memory\n");

    free(data);
    ibv_dereg_mr(data_mr);
    rmem_disconnect(&rmem);

    return 0;
}

