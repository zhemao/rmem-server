#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "util.h"

double rvm_test(int npages)
{
    double starttime, endtime;

    int *pages;
    int ints_per_page = PAGE_SIZE / sizeof(int);
    char cmd[256];
    int ret;

    pages = mmap(NULL, PAGE_SIZE * npages, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (pages == MAP_FAILED) {
	perror("mmap");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++)
	touch_page(pages + i * ints_per_page);

    snprintf(cmd, 256, "cr_checkpoint --file gen.blcr %d", getpid());
    starttime = gettime();
    ret = system(cmd);
    endtime = gettime();

    if (ret != 0) {
        fprintf(stderr, "Failed to checkpoint\n");
        exit(EXIT_FAILURE);
    }

    return endtime - starttime;
}

int main(int argc, char *argv[])
{
    int npages;
    double txn_time = 0.0;

    if (argc < 2) {
	fprintf(stderr, "Usage: %s <npages>\n", argv[0]);
	return -1;
    }

    npages = atoi(argv[1]);

    txn_time = rvm_test(npages);
    printf("%f\n", txn_time);

    return 0;
}
