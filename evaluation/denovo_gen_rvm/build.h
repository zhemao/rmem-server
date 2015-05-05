#include "kmer_hash.h"

/* Build a hash-table out of the kmer data in input_UFX_name.
 *
 * @param [out] tbl Will point to the newly created hash-table
 * @param [out] start_list Will point to a the newly created list of start kmers
 * @param [in] mem_heap Memory heap to allocate table from.
 * @param [in] input_UFX_name path to input file containing kmers in UFX format.
 * @return 1 on success, 0 otherwise.
 */
int build(hash_table_t **tbl, start_kmer_t **start_list,
        memory_heap_t *mem_heap, char *input_UFX_name);
