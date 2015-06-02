#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <cblas.h>
#include <errno.h>
#include <string.h>

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
    "\t-i NUMBER The number of iterations\n"                        \
    "\t-f PATH Optional output file\n"                              \

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

int main(int argc, char *argv[])
{
    /* Parse Inputs */
    int ncol = N_DEF, nrow = M_DEF;  /* Matrix dimensions */
    int64_t niter = NITER_DEF; /* Number of iterations */
    char *out_filename = NULL;

    int c;
    while((c = getopt(argc, argv, "n:m:i:f:")) != -1)
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
        out = fopen(out_filename, "w");
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

    /* The algorithm state. Will be preserved. */
    state_t *state = malloc(sizeof(state_t) + nrow*sizeof(double));
    if(state == NULL) {
        printf("Could not allocate vector x of dimension %d\n", nrow);
        return EXIT_FAILURE;
    }
    state->iter = 0;
    init_vec(state->vec, nrow);

    while(state->iter < niter)
    {
        cblas_dgemv(CblasColMajor, CblasNoTrans,
                nrow, ncol, 1.0, A, nrow, state->vec, 1, 0, state->vec, 1);

        state->iter++;
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
