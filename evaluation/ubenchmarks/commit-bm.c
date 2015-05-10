#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <rvm.h>
#include <buddy_malloc.h>

#include "util.h"

double rvm_test(int npages, char *host, char *port)
{
    double starttime, endtime;

    rvm_cfg_t *rvm;
    rvm_opt_t opt;
    rvm_txid_t txid;

    int *pages;
    int ints_per_page = PAGE_SIZE / sizeof(int);

    opt.host = host;
    opt.port = port;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
    opt.recovery = false;

    rvm = rvm_cfg_create(&opt, backend_layer);
    if (rvm == NULL) {
	perror("rvm_cfg_create");
	exit(EXIT_FAILURE);
    }

    txid = rvm_txn_begin(rvm);
    if (txid < 0) {
	perror("rvm_txn_begin");
	exit(EXIT_FAILURE);
    }

    pages = rvm_alloc(rvm, PAGE_SIZE * npages);
    if (pages == NULL) {
	perror("rvm_alloc");
	exit(EXIT_FAILURE);
    }

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    txid = rvm_txn_begin(rvm);
    if (txid < 0) {
	perror("rvm_txn_begin");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++)
	touch_page(pages + i * ints_per_page);

    starttime = gettime();
    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }
    endtime = gettime();

    txid = rvm_txn_begin(rvm);
    if (txid < 0) {
	perror("rvm_txn_begin");
	exit(EXIT_FAILURE);
    }

    rvm_free(rvm, pages);

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    rvm_cfg_destroy(rvm);

    return endtime - starttime;
}

int main(int argc, char *argv[])
{
    int npages;
    double txn_time = 0.0;

    if (argc < 4) {
	fprintf(stderr, "Usage: %s <host> <port> <npages>\n", argv[0]);
	return -1;
    }

    char *host = argv[1];
    char *port = argv[2];
    npages = atoi(argv[3]);

    txn_time = rvm_test(npages, host, port);
    printf("%f\n", txn_time);

    return 0;
}
