#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <cblas.h>
#include <errno.h>
#include <string.h>
#include "rvm.h"

/* Defaults for n, m and niter respectively */
#ifndef N_DEF
    #define N_DEF 5
#endif

#ifndef M_DEF
    #define M_DEF 10
#endif

#ifndef NITER_DEF
    #define NITER_DEF 5
#endif

#define USAGE \
    "Compute x*A niter times where A is an mxn random matrix and "  \
    "x is a random vector of length n. The result will be "         \
    "printed to stdout.\n"                                          \
    "Usage: ./test [-mni]"                                          \
    "\t-m NUMBER The m dimension of the matrix\n"                   \
    "\t-n NUMBER The n dimension of the matrix\n"                   \
    "\t-niter NUMBER The number of iterations\n"

typedef struct
{
    int iter;       /* Which iteration we're on */
    double vec[];   /* The latest vector */
} state_t;

/* Initialize the matrix A of dimension mxn with random numbers */
void init_mat(double *A, int m, int n);

/* Initialize the vector v of dimension m with random numbers */
void init_vec(double *v, int m);

/* Prints the contents of the vector to file "out" */
void print_vec(FILE* out, double *v, int m);

int main(int argc, char *argv[])
{
    bool res;
    rvm_txid_t txid;

    /* Parse Inputs */
    int n = N_DEF, m = M_DEF;  /* Matrix is mxn */
    int64_t niter = NITER_DEF; /* Number of iterations */
    char *host = NULL;
    char *port = NULL;
    char *out_filename = NULL;
    bool recover = false;

    int c;
    while((c = getopt(argc, argv, "n:m:i:h:p:f:r")) != -1)
    {
        switch(c) {
        case 'm':
            m = strtoll(optarg, NULL, 0);
            break;
        case 'n':
            n = strtoll(optarg, NULL, 0);
            break;
        case 'i':
            niter = strtoll(optarg, NULL, 0);
            break;
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'f':
            out_filename = optarg;
            break;
        case 'r':
            recover = true;
            break;

        case '?':
        default:
            printf(USAGE);
            return EXIT_FAILURE;
            break;
        }
    }

    if(host == NULL || port == NULL) {
        printf("Please provide a host and port number for the backup server\n");
        printf(USAGE);
        return EXIT_FAILURE;
    }

    FILE *out;
    if(out_filename == NULL) {
        out = stdout;
    } else {
        FILE *out = fopen(out_filename, "w");
        if(out == NULL) {
            printf("Failed to open output file: %s\n", out_filename);
            return EXIT_FAILURE;
        }
    }

    /* Initialize the rvm configuration */
    rvm_opt_t opt;
    opt.host = argv[1];
    opt.port = argv[2];
    opt.recovery = recover;
    rvm_cfg_t *cfg = rvm_cfg_create(&opt);
    if(cfg == NULL) {
        printf("Failed to initialize rvm: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* The input matrix */
    double *A = malloc(n*m*sizeof(double));
    if(A == NULL) {
        printf("Could not allocate matrix A of dimension %dx%d\n", n, m);
        return EXIT_FAILURE;
    }
    init_mat(A, m, n);

    /* We first try to recover state, if this is the first run or if the
     * previous run failed before initializing rvm_rec will return NULL. */
    state_t *state = rvm_rec(cfg);
    if(!state) {
        /* Starting from scratch */
        rvm_txid_t txid = rvm_txn_begin(cfg);
        if(txid == -1) {
            printf("Failed to start initial transaction\n");
            return EXIT_FAILURE;
        }

        /* The algorithm state. Will be preserved. */
        state = rvm_alloc(cfg, sizeof(state_t) + n*sizeof(double));
        if(state == NULL) {
            printf("Could not allocate vector x of dimension %d\n", n);
            return EXIT_FAILURE;
        }
        state->iter = 0;
        init_vec(state->vec, m);

        res = rvm_txn_commit(cfg, txid);
        if(!res) {
            printf("Failed to commit initial transaction\n");
            return EXIT_FAILURE;
        }
    }

    while(state->iter < niter)
    {
        txid = rvm_txn_begin(cfg);
        if(txid == -1) {
            printf("Failed to begin transaction for iteration %d\n",
                    state->iter + 1);
            return EXIT_FAILURE;
        }

        cblas_dgemv(CblasColMajor, CblasNoTrans,
                m, n, 1.0, A, m, state->vec, 1, 0, state->vec, 1);
        state->iter++;

        res = rvm_txn_commit(cfg, txid);
        if(!res) {
            printf("Failed to commit transaction for iteration %d\n",
                    state->iter);
            return EXIT_FAILURE;
        }
    }

    printf("Result:\n");
    print_vec(out, state->vec, n);

    return EXIT_SUCCESS;
}

void init_mat(double *A, int m, int n)
{
    /* Seed with constant for now for consistent results */
    srand(0);

    for(int row = 0; row < m; row++)
    {
        for(int col = 0; col < n; col++)
        {
            //A[col*n + row] = (double)rand() / (double)rand();
            A[col*n + row] = 2 * drand48() - 1;
        }
    }

    return;
}

void init_vec(double *v, int m)
{
    /* Seed with constant for now for consistent results (use different
     * constant than init_mat()) */
    srand(1);

    for(int i = 0; i < m; i++)
    {
        //v[i] = (double)rand() / (double)rand();
        v[i] = 2 * drand48() - 1;
    }

    return;
}

void print_vec(FILE* out, double *v, int m)
{
    for(int i = 0; i < m; i++)
    {
        fprintf(out, "%lf\n", v[i]);
    }
}
