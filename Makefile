.PHONY: clean

# POSIX_C_SOURCE required
# -std-c11 -D_POSIX_C_SOURCE=1
CFLAGS  := -Wall -g -fPIC 
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread

APPS    := rmem-server rmem-client rmem-test dgemv_test

all: ${APPS} rvm-test

rmem-server: common.o rmem_table.o rmem-server.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-client: common.o rmem.o rmem-client.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-test: common.o rmem_table.o rmem-test.o
	${LD} -o $@ $^ ${LDFLAGS}

dgemv_test: dgemv_test.o
	${LD} -o $@ $^ -lblas

rvm-test: common.o rmem_table.o rmem-test.o rvm.o rmem.c
	${LD} -o $@ $^ ${LDFLAGS}

clean:
	rm -f *.o ${APPS}

