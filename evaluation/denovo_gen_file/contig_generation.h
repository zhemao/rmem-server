#ifndef CONTIG_GENERATION_H
#define CONTIG_GENERATION_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>

#ifndef MAXIMUM_CONTIG_SIZE
#define MAXIMUM_CONTIG_SIZE 100000
#endif

#ifndef KMER_LENGTH
#define KMER_LENGTH 19
#endif

#ifndef LOAD_FACTOR
#define LOAD_FACTOR 1
#endif

#ifndef LINE_SIZE
#define LINE_SIZE (KMER_LENGTH+4)
#endif

static int64_t gettime(void) {
    int64_t retval;
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
	perror("gettimeofday");
	abort();
    }
    retval = ((int64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
    return retval/1000000;
}

/* K-mer data structure */
typedef struct kmer_t kmer_t;
struct kmer_t{
   char kmer[KMER_PACKED_LENGTH];
   char l_ext;
   char r_ext;
   kmer_t *next;
};

/* Start k-mer data structure */
typedef struct start_kmer_t start_kmer_t;
struct start_kmer_t{
   kmer_t *kmerPtr;
   start_kmer_t *next;
};

/* Bucket data structure */
typedef struct bucket_t bucket_t;
struct bucket_t{
   kmer_t *head;          // Pointer to the first entry of that bucket
};

/* Hash table data structure */
typedef struct hash_table_t hash_table_t;
struct hash_table_t {
   int64_t size;           // Size of the hash table
   bucket_t *table;			// Entries of the hash table are pointers to buckets
};

/* Memory heap data structure */
typedef struct memory_heap_t memory_heap_t;
struct memory_heap_t {
   kmer_t *heap;
   int64_t posInHeap;
};

int64_t getNumKmersInUFX(const char *filename);

#endif // CONTIG_GENERATION_H
