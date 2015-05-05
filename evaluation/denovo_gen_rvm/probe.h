#include "kmer_hash.h"

/* Probes the provided hash table to build a set of contigs and prints them to
 * out.
 *
 * @param [in] hash_table Hash table containing kmers.
 * @param [in] start_list List of the starting kmers for each contig
 * @param [in] out The file to write results to
 */
int probe(hash_table_t *hash_table, start_kmer_t *startKmersList, FILE *out);
