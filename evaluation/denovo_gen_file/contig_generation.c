#include "contig_generation.h"

/* Returns the number of UFX kmers in a file */
int64_t getNumKmersInUFX(const char *filename) {
   FILE *f = fopen(filename, "r");
   if (f == NULL) {
      fprintf(stderr, "Could not open %s for reading!\n", filename);
      return -1;
   }
   char firstLine[ LINE_SIZE+1 ];
   firstLine[LINE_SIZE] = '\0';
   if (fread(firstLine, sizeof(char), LINE_SIZE, f) != LINE_SIZE) {
      fprintf(stderr, "Could not read %d bytes!\n", LINE_SIZE);
      return -2;
   }
   // check structure and size of kmer is correct!
   if (firstLine[LINE_SIZE] != '\0') {
      fprintf(stderr, "UFX text file is an unexpected line length for kmer length %d\n", KMER_LENGTH);
      return -3;
   }
   if (firstLine[KMER_LENGTH] != ' ' && firstLine[KMER_LENGTH] != '\t') {
      fprintf(stderr, "Unexpected format for firstLine '%s'\n", firstLine);
      return -4;
   }

   struct stat buf;
   int fd = fileno(f);
   if (fstat(fd, &buf) != 0) {
      fprintf(stderr, "Could not stat %s\n", filename);
      return -5;
   }
   int64_t totalSize = buf.st_size;
   if (totalSize % LINE_SIZE != 0) {
      fprintf(stderr, "UFX file is not a multiple of %d bytes for kmer length %d\n", LINE_SIZE, KMER_LENGTH);
      return -6;
   }
   fclose(f);
   int64_t numKmers = totalSize / LINE_SIZE;
   printf("Detected %lld kmers in text UFX file: %s\n", numKmers, filename);
   return numKmers;
}
