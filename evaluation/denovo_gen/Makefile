CC = gcc
KMER_LENGTH 		= 19
KMER_PACKED_LENGTH 	= $(shell echo $$((($(KMER_LENGTH)+3)/4)))

#CFLAGS 	= -O3
CFLAGS 	= -g
DEFINE 	= -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH)
HEADERS	= contig_generation.h kmer_hash.h packingDNAseq.h build.h
LIBS	=
PGEN_OBJS = build.o probe.o packingDNAseq.o contig_generation.o kmer_hash.o
TARGETS	= pgen

all: 	$(TARGETS)

gen_assemble: gen_assemble.o $(PGEN_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(DEFINE) $(LIBS)

#C Objects (Some false deps on headers, oh well)
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINE) -c -o $@ $<

clean :
	rm -f *.o
	rm -rf $(TARGETS)
