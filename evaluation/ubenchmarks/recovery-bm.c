#include <stdio.h>
#include <stdlib.h>

#include <rvm.h>
#include <rmem.h>

#include "util.h"

void setup_pages(char *host, char *port, int **pages, int npages)
{
    rvm_cfg_t *rvm;
    rvm_opt_t opt;

    opt.host = host;
    opt.port = port;
    opt.recovery = false;

    rvm = rvm_cfg_create(&opt, create_rmem_layer);
    if (rvm == NULL) {
	perror("rvm_cfg_create");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++) {
	pages[i] = rvm_alloc(rvm, PAGE_SIZE);
	if (pages[i] == NULL) {
	    perror("rvm_alloc");
	    exit(EXIT_FAILURE);
	}
    }

    rvm_cfg_destroy(rvm);
}

void recover_pages(char *host, char *port,
	int **pages, int npages, bool free_at_end)
{
    rvm_cfg_t *rvm;
    rvm_opt_t opt;

    opt.host = host;
    opt.port = port;
    opt.recovery = true;

    rvm = rvm_cfg_create(&opt, create_rmem_layer);
    if (rvm == NULL) {
	perror("rvm_cfg_create");
	exit(EXIT_FAILURE);
    }

    for (int i = 0; i < npages; i++) {
	pages[i] = rvm_rec(rvm);
	if (pages[i] == NULL) {
	    perror("rvm_rec");
	    exit(EXIT_FAILURE);
	}
    }

    if (free_at_end) {
	for (int i = 0; i < npages; i++)
	    rvm_free(rvm, pages[i]);
    }

    rvm_cfg_destroy(rvm);
}

int main(int argc, char *argv[])
{
    int **pages, npages;
    double starttime, alloctime, rectime;
    char *host, *port;

    if (argc < 4) {
	fprintf(stderr, "%s <host> <port> <npages>\n", argv[0]);
	return -1;
    }

    host = argv[1];
    port = argv[2];
    npages = atoi(argv[3]);
    pages = calloc(npages, sizeof(*pages));

    starttime = gettime();
    setup_pages(host, port, pages, npages);
    alloctime = gettime() - starttime;

    starttime = gettime();
    recover_pages(host, port, pages, npages, false);
    rectime = gettime() - starttime;

    recover_pages(host, port, pages, npages, true);

    free(pages);

    printf("alloc time %f, recovery time %f\n", alloctime, rectime);

    return 0;
}
