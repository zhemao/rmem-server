CC = gcc

KMER_LENGTH 		= 19
KMER_PACKED_LENGTH 	= $(shell echo $$((($(KMER_LENGTH)+3)/4)))

CFLAGS 	  = -O3 -std=gnu11
DEFINE 	  = -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH)
HEADERS	  = contig_generation.h kmer_hash.h packingDNAseq.h
INCLUDES  =-I../.. -I../../utils/
LIBS      = -lrvm -lrdmacm -libverbs -lpthread
LINCLUDES = -L../../

TARGETS	  = serial_rvm

all: 	$(TARGETS)

serial_rvm: serial_rvm.c $(HEADERS)
		$(CC) $(CFLAGS) -o $@ $< -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH) $(INCLUDES) $(LINCLUDES) $(LIBS)

get-test:
	wget http://www.eecs.berkeley.edu/~penpornk/cs267.spr15/hw3/test.zip
	unzip test
	rm test.zip

clean :
	rm -f *.o
	rm -rf $(TARGETS)
