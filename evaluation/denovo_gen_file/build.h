#ifndef _BUILD_H_
#define _BUILD_H_

#include "kmer_hash.h"

typedef struct
{
    int64_t kmerx;
} build_state_t;

/* Build a hash-table out of the kmer data in input_UFX_name.
 *
 * @param [out] tbl Will point to the newly created hash-table
 * @param [out] start_list Will point to a the newly created list of start kmers
 * @param [inout] mem_heap Memory heap to allocate table from.
 * @param [in] input_UFX_name path to input file containing kmers in UFX format.
 * @return 1 on success, 0 otherwise.
 */
int build(gen_state_t *state, char *input_UFX_name);

/* Free the build-specific state left in state->pstate after build */
void build_state_free(void *build_state);
#endif
