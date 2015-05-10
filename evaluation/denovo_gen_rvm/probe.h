#ifndef _PROBE_H_
#define _PROBE_H_

#include "kmer_hash.h"

typedef struct
{
    int64_t nkmer_proc; /* Number of kmers processed so far */
    int64_t skmerx;
    char cur_contig[MAXIMUM_CONTIG_SIZE];
    start_kmer_t *curStartNode;
    kmer_t *cur_kmr;
    int64_t posInContig;
    size_t floc; /* Next writable location in output file */
} probe_state_t;

/* Probes the provided hash table to build a set of contigs and prints them to
 * out.
 *
 * @param [in] hash_table Hash table containing kmers.
 * @param [in] start_list List of the starting kmers for each contig
 * @param [in] out The file to write results to
 */
int probe(gen_state_t *state, FILE *out);

/* Free the probe-specific state */
void probe_state_free(void *pstate);

#endif
