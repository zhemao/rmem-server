INCLUDES := -I../..
CFLAGS  := $(INCLUDES) -Wall -g -std=gnu11
LIBS := -lrvm -lrdmacm -libverbs -lpthread
LDFLAGS := -L../.. -std=gnu11 $(LIBS)
LD := gcc

BENCHMARKS := commit-bm recovery-bm

all: $(BENCHMARKS)

commit-bm: commit-bm.o
	$(LD) $^ -o $@ $(LDFLAGS)

recovery-bm: recovery-bm.o
	$(LD) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(BENCHMARKS) *.o
