#ifndef __STATS_H
#define __STATS_H

#include <sys/time.h>
#include <assert.h>
#include "../utils/error.h"
#include "../data/stack.h"

#define STATS_ENABLED // comment to turn make stat calls free

#define US_IN_SEC (1000*1000)

enum kstats_vector { KSTATS_PRE_CONN, KSTATS_ON_CONN, KSTATS_ON_COMPL, 
    KSTATS_SEND_MSG, KSTATS_POST_MSG_RECV, KSTATS_MAX};

#define KSTATS_STRINGS {"none", "rmem_server_pre_conn", "rmem_server_on_conn", \
    "rmem_server_on_compl", "rmem_server_send_msg", "rmem_server_post_msg_recv", \
    "kstats-end"}

typedef struct stats_distr {
    uint64_t n;
    uint64_t min;
    uint64_t max;
    uint64_t tot;
} stats_distr_t;

typedef struct stat_entry {
    struct timeval time;
    enum kstats_vector kvec;	
} stat_entry_t;

typedef struct stats { 
    stats_distr_t kstats[KSTATS_MAX];
    stack_p stats_stack;
} stats_t;

extern struct stats stats;

static inline void KSTATS_RECORD(int kvec, uint64_t val)
{
    stats_distr_t *k = &stats.kstats[kvec];
    if (kvec < 0 || kvec > KSTATS_MAX ) {
        fprintf(stderr, "wrong kvec %d\n", kvec);
        exit(-1);
    }
    assert (kvec >= 0 && kvec < KSTATS_MAX);
    if (k->n == 0) { 
        k->min = val;
        k->max = val;
    } else {
        if (k->min>val) k->min = val;
        if (k->max<val) k->max = val;
    }
    k->n++;
    k->tot += val;
}

static stat_entry_t*
create_stat_entry(enum kstats_vector kvec, struct timeval time)
{
    stat_entry_t* se = malloc(sizeof(stat_entry_t));
    CHECK_ERROR(se == 0,
            ("Error: allocating stat_entry_t\n"));

    se->kvec = kvec;
    se->time = time;
    return se;
}

static inline
void stats_start(enum kstats_vector kvec)
{
#ifdef STATS_ENABLED
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    stat_entry_t* se = create_stat_entry(kvec, start_time);
    stack_push(stats.stats_stack, se);
#endif
}

static inline
void stats_end(enum kstats_vector kvec) {
#ifdef STATS_ENABLED
    CHECK_ERROR(stack_size(stats.stats_stack) <= 0,
            ("Error: stats_end requires a previous call to stats_start\n"))

    stat_entry_t* se = stack_pop(stats.stats_stack);

    assert(se->kvec == kvec);

    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    uint64_t sec_delta = end_time.tv_sec - se->time.tv_sec;
    uint64_t usec_delta = end_time.tv_usec - se->time.tv_usec;

    uint64_t us_delta = (sec_delta * US_IN_SEC + usec_delta);

    KSTATS_RECORD(kvec, us_delta);

    free(se);
#endif
}

void stats_dump()
{
#ifdef STATS_ENABLED
    char *kstats_strings[] = KSTATS_STRINGS;

    uint64_t tot = 0; 

    for (int i = 0; i < KSTATS_MAX; i++) { 
        tot += stats.kstats[i].tot;
    }    

    printf("\n");
    for (int i = 0; i < KSTATS_MAX; i++) { 
        stats_distr_t *k = &stats.kstats[i];
        if (k->n == 0)
            continue; // ignore unused stats
        printf("KSTATS\t\t %-30s %4ld%% %10ld %10ld/%10ld/%10ld\n",kstats_strings[i],
                (tot > 0 ? 100 * k->tot / tot : -1),
                k->n, k->min, (k->n > 0 ? (k->tot / k->n) : -1), k->max);
    }    
#endif
}

void stats_init() {
    stats.stats_stack = stack_create();
}

void stats_destroy() {
    stack_destroy(&stats.stats_stack);
}

#endif

