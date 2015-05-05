#ifndef __UBM_UTIL_H__
#define __UBM_UTIL_H__

#include <unistd.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)
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

#endif