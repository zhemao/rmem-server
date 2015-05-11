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

int fa_freq;
int cp_freq;

void initialize_rvm(char*host, char* port, bool rec)
{
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;
    opt.recovery = rec;
    opt.nentries = (1 << 16);

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
        state->memheap = rvm_alloc(cfg, sizeof(memory_heap_t));
        CHECK_ERROR(state->memheap == NULL, ("Failed to allocate mem heap\n"));
        state->tbl = NULL;
        state->start_list = NULL;
        state->pstate = NULL;
        state->phase = BUILD; /* First phase */

        /* Save state in rvm layer */
        rvm_set_usr_data(cfg, state);

        TX_COMMIT(txid);

        init_mem_sz = rvm_get_alloc_sz(cfg);
        printf("Initialized State, starting build phase\n");
        printf("Initial recoverable memory: %ld\n", init_mem_sz);
    }


    /* Initialize the lookup table for kmer packing */
    init_LookupTable();
    
    init_time += gettime();

    switch(state->phase) {
    case BUILD:
        /** Graph construction **/
        build_time -= gettime();

        build(state, input_UFX_name);
        build_mem_sz = rvm_get_alloc_sz(cfg) - init_mem_sz;

        printf("Built\n");
        printf("Build Recoverable Memory: %ld\n", build_mem_sz);

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

        probe_mem_sz = rvm_get_alloc_sz(cfg) - init_mem_sz - build_mem_sz;

        printf("Probed\n");
        printf("Probe Recoverable Memory: %ld\n", probe_mem_sz);

        probe_time += gettime();
        state->phase = DONE;
        probe_state_free(state->pstate);
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
    printf("Total Recoverable Memory: %ld\n", rvm_get_alloc_sz(cfg));
	printf("%s: Input set: %s\n", argv[0], argv[1]);
	printf("Initialization time: %f seconds\n", init_time);
	printf("Build time: %f seconds\n", build_time);
	printf("Build time: %f seconds\n", probe_time);
	return 0;
}
