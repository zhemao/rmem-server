#include <stdio.h>
#include <stdlib.h>

#include <rvm.h>
#include <rmem.h>
#include <buddy_malloc.h>

#include "util.h"

void setup_pages(char *host, char *port, int **pages, int npages)
{
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
	touch_page(pages[i]);
    }

    if (!rvm_txn_commit(rvm, txid)) {
	perror("rvm_txn_commit");
	exit(EXIT_FAILURE);
    }

    rvm_cfg_destroy(rvm);
}

double recover_pages(char *host, char *port, int **pages, int npages)
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
    rvm = rvm_cfg_create(&opt, create_rmem_layer);
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
    char *host, *port;
    double rectime;

    if (argc < 4) {
	fprintf(stderr, "%s <host> <port> <npages>\n", argv[0]);
	return -1;
    }

    host = argv[1];
    port = argv[2];
    npages = atoi(argv[3]);
    pages = calloc(npages, sizeof(*pages));

    setup_pages(host, port, pages, npages);
    rectime = recover_pages(host, port, pages, npages);

    free(pages);

    printf("%f\n", rectime);

    return 0;
}
