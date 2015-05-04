INCLUDES := -I../..
CFLAGS  := $(INCLUDES) -Wall -O3 -std=gnu11
LIBS := -lrvm -lrdmacm -libverbs -lpthread
LDFLAGS := -L../.. -std=gnu11 $(LIBS)
LD := gcc

BENCHMARKS := commit-bm

commit-bm: commit-bm.o
	$(LD) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(BENCHMARKS) *.o
