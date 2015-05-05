#include "build.h"
#include "packingDNAseq.h"

int build(hash_table_t **tbl, start_kmer_t **start_list,
        memory_heap_t *mem_heap, char *input_UFX_name)
{
   /* Initialize lookup table that will be used for the DNA packing routines */
   init_LookupTable();

   /* Extract the number of k-mers in the input file */
   int64_t nKmers = getNumKmersInUFX(input_UFX_name);
   hash_table_t *hashtable;

   /* Create a hash table */
   hashtable = create_hash_table(nKmers, mem_heap);

   /* Read the kmers from the input file and store them in the working_buffer */
   int64_t total_chars_to_read = nKmers * LINE_SIZE;
   unsigned char *working_buffer = (unsigned char*) malloc(total_chars_to_read * sizeof(unsigned char));
   FILE *inputFile = fopen(input_UFX_name, "r");
   int64_t cur_chars_read = fread(working_buffer, sizeof(unsigned char),total_chars_to_read , inputFile);
   fclose(inputFile);

   /* Process the working_buffer and store the k-mers in the hash table */
   /* Expected format: KMER LR ,i.e. first k characters that represent the kmer, then a tab and then two chatacers, one for the left (backward) extension and one for the right (forward) extension */

   int64_t ptr = 0;
   while (ptr < cur_chars_read) {
      /* working_buffer[ptr] is the start of the current k-mer                */
      /* so current left extension is at working_buffer[ptr+KMER_LENGTH+1]    */
      /* and current right extension is at working_buffer[ptr+KMER_LENGTH+2]  */

      char left_ext = (char) working_buffer[ptr+KMER_LENGTH+1];
      char right_ext = (char) working_buffer[ptr+KMER_LENGTH+2];

      /* Add k-mer to hash table */
      add_kmer(hashtable, mem_heap, &working_buffer[ptr], left_ext, right_ext);

      /* Create also a list with the "start" kmers: nodes with F as left (backward) extension */
      if (left_ext == 'F') {
         addKmerToStartList(mem_heap, start_list);
      }

      /* Move to the next k-mer in the input working_buffer */
      ptr += LINE_SIZE;
   }

   *tbl = hashtable;
   return 1;
}

