#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <cblas.h>

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

/* Initialize the matrix A of dimension mxn with random numbers */
void init_mat(double *A, int m, int n);

/* Initialize the vector v of dimension m with random numbers */
void init_vec(double *v, int m);

/* Prints the contents of the vector to file "out" */
void print_vec(FILE* out, double *v, int m);

int main(int argc, char *argv[])
{
    /* Parse Inputs */
    int n = N_DEF, m = M_DEF;  /* Matrix is mxn */
    int64_t niter = NITER_DEF; /* Number of iterations */

    int c;
    while((c = getopt(argc, argv, "n:m:i:")) != -1)
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

        case '?':
        case 'h':
        default:
            printf("Compute x*A niter times where A is an mxn random matrix and"
                    "x is a random vector of length n. The result will be"
                    "printed to stdout.\n");
            printf("Usage: ./%s [-mni]", argv[0]);
            printf("\t-m NUMBER The m dimension of the matrix\n");
            printf("\t-n NUMBER The n dimension of the matrix\n");
            printf("\t-niter NUMBER The number of iterations\n");
            break;
        }
    }

    /* The input matrix */
    double *A = malloc(n*m*sizeof(double));
    if(A == NULL) {
        printf("Could not allocate matrix A of dimension %dx%d\n", n, m);
        return EXIT_FAILURE;
    }
    init_mat(A, m, n);

    /* The working vector */
    double *x = malloc(n*sizeof(double));
    if(x == NULL) {
        printf("Could not allocate vector x of dimension %d\n", n);
        return EXIT_FAILURE;
    }
    init_vec(x, m);

    for(int iter = 0; iter < niter; iter++)
    {
        cblas_dgemv(CblasColMajor, CblasNoTrans,
                m, n, 1.0, A, m, x, 1, 0, x, 1);
    }

    printf("Result:\n");
    print_vec(stdout, x, n);

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
