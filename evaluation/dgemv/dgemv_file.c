#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <cblas.h>
#include <errno.h>
#include <string.h>
#include "rvm.h"
#include "buddy_malloc.h"

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

#define REC_FILE "dgemv.rec"
#define REC_TMP_FILE "dgemv_tmp.rec"

#define USAGE \
    "Compute x*A niter times where A is an mxn random matrix and "  \
    "x is a random vector of length n. The result will be "         \
    "printed to stdout.\n"                                          \
    "Usage: ./test [-mni]"                                          \
    "\t-m NUMBER The m dimension of the matrix\n"                   \
    "\t-n NUMBER The n dimension of the matrix\n"                   \
    "\t-i NUMBER The number of iterations\n"                        \
    "\t-f PATH Optional output file\n"                              \
    "\t-s NUM Shall we simulate a failure every NUM iterations?\n"  \
    "\t-c NUM Commit frequency\n"

typedef struct
{
    int iter;       /* Which iteration we're on */
    double vec[];   /* The latest vector */
} state_t;

/* Initialize the matrix A of dimension mxn with random numbers */
void init_mat(double *A, int nrow, int ncol);

/* Initialize the vector v of dimension m with random numbers */
void init_vec(double *v, int m);

/* Prints the contents of the vector to file "out" */
void print_vec(FILE* out, double *v, int m);

/* Commit the state atomically to a file */
bool commit_state(state_t *state, size_t size);

int main(int argc, char *argv[])
{
    bool res;
    rvm_txid_t txid;

    /* Parse Inputs */
    int ncol = N_DEF, nrow = M_DEF;  /* Matrix dimensions */
    int64_t niter = NITER_DEF; /* Number of iterations */
    char *out_filename = NULL;
    int fail_freq = 0;
    int cp_freq = 1;

    int c;
    while((c = getopt(argc, argv, "n:m:i:f:rs:c:r")) != -1)
    {
        switch(c) {
        case 'm':
            nrow = strtoll(optarg, NULL, 0);
            break;
        case 'n':
            ncol = strtoll(optarg, NULL, 0);
            break;
        case 'i':
            niter = strtoll(optarg, NULL, 0);
            break;
        case 'f':
            out_filename = optarg;
            break;
        case 's':
            fail_freq = strtoll(optarg, NULL, 0);
            break;
        case 'c':
            cp_freq = strtoll(optarg, NULL, 0);
            break;

        case 'r':
            break;
        case '?':
        default:
            printf(USAGE);
            return EXIT_FAILURE;
            break;
        }
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

    /* The input matrix */
    double *A = malloc(ncol*nrow*sizeof(double));
    if(A == NULL) {
        printf("Could not allocate matrix A of dimension %dx%d\n", nrow, ncol);
        return EXIT_FAILURE;
    }
    init_mat(A, nrow, ncol);

    /* Allocate space for program state */
    size_t state_sz = sizeof(state_t) + nrow*sizeof(double);
    state_t *state = malloc(state_sz);
    if(state == NULL) {
        printf("Failed to allocate state\n");
        return EXIT_FAILURE;
    }

    /* Try to recover the state if a recovery file is found */
    if(access(REC_FILE, F_OK) != -1) {
        FILE *rec = fopen(REC_FILE, "r");
        if(rec == NULL) {
            printf("Failed to open recovery file\n");
            return EXIT_FAILURE;
        }

        if(fread(state, state_sz, 1, rec) != 1) {
            printf("Failed to read recovery file\n");
            return EXIT_FAILURE;
        }

        fclose(rec);

    } else {
        state->iter = 0;
        init_vec(state->vec, nrow);
        commit_state(state, state_sz);
    }

    while(state->iter < niter)
    {
        cblas_dgemv(CblasColMajor, CblasNoTrans,
                nrow, ncol, 1.0, A, nrow, state->vec, 1, 0, state->vec, 1);

        state->iter++;

        if(state->iter % cp_freq == 0) {
            if(!commit_state(state, state_sz)) {
                printf("Failed to commit iteration %d\n", state->iter);
                return EXIT_FAILURE;
            }
        }

        //Hard-coded failure
        if(fail_freq != 0 && (state->iter % fail_freq) == 0) {
            printf("Simulating failure after %dth iteration\n", state->iter);
            return EXIT_SUCCESS;
        }
    }

    printf("Result:\n");
    print_vec(out, state->vec, nrow);

    return EXIT_SUCCESS;
}

void init_mat(double *A, int nrow, int ncol)
{
    /* Seed with constant for now for consistent results */
    srand(0);

    for(int row = 0; row < nrow; row++)
    {
        for(int col = 0; col < ncol; col++)
        {
            A[col*nrow + row] = 2 * drand48() - 1;
        }
    }

    return;
}

void init_vec(double *v, int nrow)
{
    /* Seed with constant for now for consistent results (use different
     * constant than init_mat()) */
    srand(1);

    for(int i = 0; i < nrow; i++)
    {
        //v[i] = (double)rand() / (double)rand();
        v[i] = 2 * drand48() - 1;
    }

    return;
}

void print_vec(FILE* out, double *v, int nrow)
{
    for(int i = 0; i < nrow; i++)
    {
        fprintf(out, "%lf\n", v[i]);
    }
}

bool commit_state(state_t *state, size_t size)
{
    /* Open temporary file, this truncates whatever used to be there */
    FILE *rec = fopen(REC_TMP_FILE, "w");

    /* Copy state to temporary file */
    if(fwrite(state, size, 1, rec) != 1) {
        printf("Failed to write state\n");
        return false;
    }

    /* Flush everything to disk */
    fflush(rec);
    int fd = fileno(rec);
    fsync(fd);
    fclose(rec);

    /* Atomically swap the tmp to the permanent */
    if(rename(REC_TMP_FILE, REC_FILE)  == -1) {
        printf("Failed to commit changes\n");
        return false;
    }

    return true;
}
