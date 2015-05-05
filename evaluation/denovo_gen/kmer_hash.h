#ifndef KMER_HASH_H
#define KMER_HASH_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h> 
#include <string.h>
#include "contig_generation.h"
#include "packingDNAseq.h"

/* Creates a hash table and (pre)allocates memory for the memory heap */
hash_table_t* create_hash_table(int64_t nEntries, memory_heap_t *memory_heap);

/* Auxiliary function for computing hash values */
int64_t hashseq(int64_t  hashtable_size, char *seq, int size);

/* Returns the hash value of a kmer */
int64_t hashkmer(int64_t  hashtable_size, char *seq);

/* Looks up a kmer in the hash table and returns a pointer to that entry */
kmer_t* lookup_kmer(hash_table_t *hashtable, const unsigned char *kmer);

/* Adds a kmer and its extensions in the hash table (note that a memory heap should be preallocated. ) */
int add_kmer(hash_table_t *hashtable, memory_heap_t *memory_heap, const unsigned char *kmer, char left_ext, char right_ext);

/* Adds a k-mer in the start list by using the memory heap (the k-mer was "just added" in the memory heap at position posInHeap - 1) */
void addKmerToStartList(memory_heap_t *memory_heap, start_kmer_t **startKmersList);

/* Deallocation functions */
int dealloc_heap(memory_heap_t *memory_heap);

int dealloc_hashtable(hash_table_t *hashtable);

#endif // KMER_HASH_H
