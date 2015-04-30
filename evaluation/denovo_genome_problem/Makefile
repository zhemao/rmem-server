CC = g++
UPCC = upcc

KMER_LENGTH 		= 19
KMER_PACKED_LENGTH 	= $(shell echo $$((($(KMER_LENGTH)+3)/4)))

CFLAGS 	  = -O3 -std=c++0x
DEFINE 	  = -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH)
HEADERS	  = contig_generation.h kmer_hash.h packingDNAseq.h
INCLUDES  = -I/nscratch/joao/ramcloud/src/ -I/nscratch/joao/ramcloud/ -I/nscratch/joao/ramcloud/obj.master 
LINCLUDES = -L/nscratch/joao/ramcloud/obj.master
LIBS      = -lramcloud -lboost_system -lboost_program_options
OBJ_PATH  = /nscratch/joao/ramcloud/obj.master
RC_OBJS   = ${OBJ_PATH}/OptionParser.o

TARGETS	  = serial serial_ramcloud

all: 	$(TARGETS)

serial: serial.c $(HEADERS) 
		$(CC) $(CFLAGS) -o $@ $^ -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH)

serial_ramcloud: serial_ramcloud.c $(HEADERS)
		$(CC) $(CFLAGS) -o $@ $< -DKMER_LENGTH=$(KMER_LENGTH) -DKMER_PACKED_LENGTH=$(KMER_PACKED_LENGTH) $(LINCLUDES) $(LIBS) $(INCLUDES) 

get-test:
	wget http://www.eecs.berkeley.edu/~penpornk/cs267.spr15/hw3/test.zip
	unzip test
	rm test.zip

clean :
	rm -f *.o
	rm -rf $(TARGETS)
