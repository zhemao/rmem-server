#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rvm.h>
#include <rmem.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#define ITERATIONS 1000
#define NS_PER_SEC (1000.0 * 1000.0 * 1000.0)

static inline double gettime(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
	perror("clock_gettime");
	exit(EXIT_FAILURE);
    }

    return ts.tv_sec + ((double) ts.tv_nsec) / NS_PER_SEC;
}

static inline void touch_page(int *page)
{
    int page_len = PAGE_SIZE / sizeof(int);

    for (int i = 0; i < page_len; i++)
	page[i] = random();
}

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

    for (int i = 0; i < npages; i++)
	rvm_free(rvm, pages[i]);

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

    printf("Avg. commit time %f seconds\n", txn_time);

    free(pages);

    return 0;
}
