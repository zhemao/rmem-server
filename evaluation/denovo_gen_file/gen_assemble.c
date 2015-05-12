#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <buddy_malloc.h>
//#include <backends/rmem_generic_interface.h>
#include <backends/rmem_backend.h>
#include <backends/stub_backend.h>
#include "common.h"
#include "build.h"
#include "probe.h"
#include "packingDNAseq.h"
#include "kmer_hash.h"

#define USAGE                                                       \
    "Usage: ./gen_assemble [-ihpfrs]\n"                              \
    "\t-i PATH Path to input file.\n"                               \
    "\t-h NAME The hostname of the server\n"                        \
    "\t-p NUMBER The port used by the server"                       \
    "\t-f PATH Optional output file\n"                              \
    "\t-r Are we recovering from a failure?\n"                      \
    "\t-s NUM Shall we simulate a failure every NUM kmers?\n"       \
    "\t-c NUM Commit Frequency\n"

const char DEF_OUT_NAME[8] = "gen.out";

int fa_freq;
int cp_freq;

int main(int argc, char *argv[]) {

	/** Declarations **/
	char *input_UFX_name = NULL;
	char *hostname = NULL;
	char *port = NULL;
	const char *out_filename = DEF_OUT_NAME;
	bool rec = false;
    size_t init_mem_sz = 0;
    size_t build_mem_sz = 0;
    size_t probe_mem_sz = 0;

    double init_time = 0;
    double build_time = 0;
    double probe_time = 0;

    /* Pick defaults that result in a clean baseline */
    fa_freq  = INT_MAX;
    cp_freq = INT_MAX;

    init_time -= gettime();

	/* Parse Args */
    int c;
    while((c = getopt(argc, argv, "n:m:i:h:p:f:rs:c:")) != -1)
    {
        switch(c) {
        case 'i':
            input_UFX_name = optarg;
            break;
        case 'h':
            hostname = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'f':
            out_filename = optarg;
            break;
        case 'r':
            rec = true;
            break;
        case 's':
            fa_freq = strtoll(optarg, NULL, 0);
            break;
        case 'c':
            cp_freq = strtoll(optarg, NULL, 0);
            break;

        case '?':
        default:
            printf(USAGE);
            return EXIT_FAILURE;
            break;
        }
    }
    CHECK_ERROR(input_UFX_name == NULL,
            ("Please provide an input file\n" USAGE));

    gen_state_t *state = NULL;
    if(state == NULL) {
        state = malloc(sizeof(gen_state_t));
        CHECK_ERROR(state == NULL, ("Failed to allocate initial state\n"));

        /* initialize state */
        state->memheap = malloc(sizeof(memory_heap_t));
        CHECK_ERROR(state->memheap == NULL, ("Failed to allocate mem heap\n"));
        state->tbl = NULL;
        state->start_list = NULL;
        state->pstate = NULL;
        state->phase = BUILD; /* First phase */

        TX_COMMIT(0);
    }


    /* Initialize the lookup table for kmer packing */
    init_LookupTable();
    
    init_time += gettime();
	printf("Initialization time: %f seconds\n", init_time);

    switch(state->phase) {
    case BUILD:
        /** Graph construction **/
        build_time -= gettime();

        build(state, input_UFX_name);

        printf("Built\n");

        build_time += gettime();
        build_state_free(state->pstate);
        state->pstate = NULL;
        state->phase = PROBE;

    /* fall through */
    case PROBE:
        /** Graph traversal **/
        probe_time -= gettime();

        FILE *out = fopen(out_filename, "w");
        probe(state, out);
        fclose(out);

        printf("Probed\n");

        probe_time += gettime();
        state->phase = DONE;
        probe_state_free(state->pstate);
        break;

    default:
        printf("Recovered to invalid state!\n");
        return EXIT_FAILURE;
    }

	/** Print timing and output info **/
	printf("%s: Input set: %s\n", argv[0], argv[1]);
	printf("Build time: %f seconds\n", build_time);
	printf("Probe time: %f seconds\n", probe_time);
	return 0;
}
