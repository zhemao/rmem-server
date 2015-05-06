#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <rvm.h>
#include <rmem.h>
#include <buddy_malloc.h>

#include "util.h"

#define ITERATIONS 1000

/* A test without RVM as the baseline */
double norvm_test(int **pages, int npages)
{
    double starttime, endtime;

    for (int i = 0; i < npages; i++) {
	pages[i] = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (pages[i] == MAP_FAILED) {
	    perror("mmap");
	    exit(EXIT_FAILURE);
	}
    }

    starttime = gettime();

    for (int j = 0; j < ITERATIONS; j++) {
	for (int i = 0; i < npages; i++)
	    touch_page(pages[i]);
    }

    endtime = gettime();

    for (int i = 0; i < npages; i++) {
	munmap(pages[i], PAGE_SIZE);
    }

    return endtime - starttime;
}

/* A test with RVM */
double rvm_test(int **pages, int npages, char *host, char *port)
{
    double starttime, endtime;

    rvm_cfg_t *rvm;
    rvm_opt_t opt;
    rvm_txid_t txid;

    opt.host = host;
    opt.port = port;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
    opt.recovery = false;

    rvm = rvm_cfg_create(&opt, create_rmem_layer);
    if (rvm == NULL) {
	perror("rvm_cfg_create");
	exit(EXIT_FAILURE);
    }

    txid = rvm_txn_begin(rvm);
    if (txid < 0) {
	perror("rvm_txn_begin");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++) {
	pages[i] = rvm_alloc(rvm, PAGE_SIZE);
	if (pages[i] == NULL) {
	    perror("rvm_alloc");
	    exit(EXIT_FAILURE);
	}
    }

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    starttime = gettime();

    for (int j = 0; j < ITERATIONS; j++) {
	txid = rvm_txn_begin(rvm);
	if (txid < 0) {
	    perror("rvm_txn_begin");
	    exit(EXIT_FAILURE);
	}
	for (int i = 0; i < npages; i++)
	    touch_page(pages[i]);
	if (!rvm_txn_commit(rvm, txid)) {
	    perror("rvm_txn_commit");
	    exit(EXIT_FAILURE);
	}
    }

    endtime = gettime();

    txid = rvm_txn_begin(rvm);
    if (txid < 0) {
	perror("rvm_txn_begin");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++)
	rvm_free(rvm, pages[i]);

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    rvm_cfg_destroy(rvm);

    return endtime - starttime;
}

int main(int argc, char *argv[])
{
    int **pages, npages;
    double t1, t2, txn_time;

    if (argc < 4) {
	fprintf(stderr, "Usage: %s <host> <port> <npages>\n", argv[0]);
	return -1;
    }

    npages = atoi(argv[3]);
    pages = calloc(npages, sizeof(*pages));

    t1 = norvm_test(pages, npages);
    t2 = rvm_test(pages, npages, argv[1], argv[2]);
    txn_time = (t2 - t1) / ITERATIONS;

    printf("%d,%f\n", npages, txn_time);

    free(pages);

    return 0;
}
