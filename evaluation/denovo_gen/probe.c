#include "kmer_hash.h"
#include "packingDNAseq.h"

int probe(hash_table_t *hashtable, start_kmer_t *startKmersList, FILE *out)
{

    char cur_contig[MAXIMUM_CONTIG_SIZE], unpackedKmer[KMER_LENGTH+1];

    /* Pick start nodes from the startKmersList */
    start_kmer_t *curStartNode = startKmersList;

    int64_t contigID = 0, totBases = 0;
    while (curStartNode != NULL ) {
       /* Need to unpack the seed first */
       kmer_t *cur_kmer_ptr = curStartNode->kmerPtr;
       unpackSequence((unsigned char*) cur_kmer_ptr->kmer,  (unsigned char*) unpackedKmer, KMER_LENGTH);

       /* Initialize current contig with the seed content */
       memcpy(cur_contig ,unpackedKmer, KMER_LENGTH * sizeof(char));
       int64_t posInContig = KMER_LENGTH;
       char right_ext = cur_kmer_ptr->r_ext;

       /* Keep adding bases while not finding a terminal node */
       while (right_ext != 'F') {
          cur_contig[posInContig] = right_ext;
          posInContig++;
          /* At position cur_contig[posInContig-KMER_LENGTH] starts the last k-mer in the current contig */
          cur_kmer_ptr = lookup_kmer(hashtable, (const unsigned char *) &cur_contig[posInContig-KMER_LENGTH]);
          right_ext = cur_kmer_ptr->r_ext;
       }

       /* Print the contig since we have found the corresponding terminal node */
       cur_contig[posInContig] = '\0';
       fprintf(out,"%s\n", cur_contig);
       contigID++;
       totBases += strlen(cur_contig);

       /* Move to the next start node in the list */
       curStartNode = curStartNode->next;
    }

    return 1;
}
