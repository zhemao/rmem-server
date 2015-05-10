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

rvm_cfg_t *cfg;

void initialize_rvm(char*host, char* port, bool rec)
{
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;
    opt.recovery = rec;

    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;

    cfg = rvm_cfg_create(&opt, create_rmem_layer);
//    cfg = rvm_cfg_create(&opt, create_stub_layer);
}

int main(int argc, char *argv[]) {

	/** Declarations **/
	char *input_UFX_name = NULL;
	rvm_txid_t txid;
	char *hostname = NULL;
	char *port = NULL;
	const char *out_filename = DEF_OUT_NAME;
	bool rec = false;
    int fa_freq  = INT_MAX;
    int cp_freq = 1;

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
    CHECK_ERROR(hostname == NULL, ("Please provide a hostname\n" USAGE));
    CHECK_ERROR(port == NULL, ("Please provide a port\n" USAGE));
    CHECK_ERROR(input_UFX_name == NULL,
            ("Please provide an input file\n" USAGE));

	/* RVM Setup */
    printf("connecting to \"%s %s\"\n", hostname, port);
    initialize_rvm(hostname, port, rec);
    CHECK_ERROR(cfg == NULL, ("Failed to initialize rvm\n"));

    /* Check if there is an initial state to recover from */
    gen_state_t *state = rvm_get_usr_data(cfg);
    if(state == NULL) {
        TX_START(txid);
        state = rvm_alloc(cfg, sizeof(gen_state_t));
        CHECK_ERROR(state == NULL, ("Failed to allocate initial state\n"));

        /* initialize state */
        state->cp_freq = cp_freq;
        state->fa_freq = fa_freq;
        state->memheap = rvm_alloc(cfg, sizeof(memory_heap_t));
        CHECK_ERROR(state->memheap == NULL, ("Failed to allocate mem heap\n"));
        state->tbl = NULL;
        state->start_list = NULL;
        state->constrTime = state->inputTime = state->traversalTime = 0.0;
        state->pstate = NULL;
        state->phase = BUILD; /* First phase */

        TX_COMMIT(txid);
        printf("Initialized State, starting build phase\n");
    }

    switch(state->phase) {
    case BUILD:
        /** Graph construction **/
        TX_START(txid);
        state->constrTime -= gettime();
        TX_COMMIT(txid);

        build(state, input_UFX_name);

        printf("Built\n");
        TX_START(txid);
        state->constrTime += gettime();
        build_state_free(state->pstate);
        state->pstate = NULL;
        state->phase = PROBE;
        TX_COMMIT(txid);

    /* fall through */
    case PROBE:
        /** Graph traversal **/
        TX_START(txid);
        state->traversalTime -= gettime();
        TX_COMMIT(txid);

        FILE *out = fopen(out_filename, "w");
        probe(state, out);
        fclose(out);

        printf("Probed\n");
        TX_START(txid);
        state->traversalTime += gettime();
        state->phase = DONE;
        probe_state_free(state->pstate);
        TX_COMMIT(txid);
        break;

    default:
        printf("Recovered to invalid state!\n");
        return EXIT_FAILURE;
    }

	/* Clean up */
    //TX_START(txid);
    //dealloc_heap(state->memheap);
    //TX_COMMIT(txid);

	/** Print timing and output info **/
	/***** DO NOT CHANGE THIS PART ****/
	printf("%s: Input set: %s\n", argv[0], argv[1]);
	printf("Input reading time: %f seconds\n", state->inputTime);
	printf("Graph construction time: %f seconds\n", state->constrTime);
	printf("Graph traversal time: %f seconds\n", state->traversalTime);
	return 0;
}
