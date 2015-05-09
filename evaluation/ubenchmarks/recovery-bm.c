#include <stdio.h>
#include <stdlib.h>

#include <rvm.h>
#include <buddy_malloc.h>

#include "util.h"

int *setup_pages(char *host, char *port, int npages)
{
    rvm_cfg_t *rvm;
    rvm_opt_t opt;
    rvm_txid_t txid;

    opt.host = host;
    opt.port = port;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
    opt.recovery = false;

    int *pages;

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
	fprintf(stderr, "Failed to allocate pages\n");
	exit(EXIT_FAILURE);
    }

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    rvm_cfg_destroy(rvm);

    return pages;
}

double recover_pages(char *host, char *port, int *pages, int npages)
{
    double starttime, endtime;

    rvm_cfg_t *rvm;
    rvm_opt_t opt;

    opt.host = host;
    opt.port = port;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
    opt.recovery = true;

    starttime = gettime();
    rvm = rvm_cfg_create(&opt, backend_layer);
    endtime = gettime();
    if (rvm == NULL) {
	perror("rvm_cfg_create");
	exit(EXIT_FAILURE);
    }

    rvm_txid_t txid = rvm_txn_begin(rvm);
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
    int *pages, npages;
    char *host, *port;
    double rectime;

    if (argc < 4) {
	fprintf(stderr, "%s <host> <port> <npages>\n", argv[0]);
	return -1;
    }

    host = argv[1];
    port = argv[2];
    npages = atoi(argv[3]);

    pages = setup_pages(host, port, npages);
    rectime = recover_pages(host, port, pages, npages);

    printf("%f\n", rectime);

    return 0;
}
