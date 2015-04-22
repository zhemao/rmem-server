.PHONY: clean

CFLAGS  := -Wall -g -fPIC -std=gnu11
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -libverbs -lpthread

APPS    := rmem-server rmem-client dgemv_test

all: ${APPS}

rmem-server: common.o rmem_table.o rmem-server.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-client: common.o rmem.o rmem-client.o
	${LD} -o $@ $^ ${LDFLAGS}

rmem-test: common.o rmem_table.o rmem-test.o
	${LD} -o $@ $^ ${LDFLAGS}

dgemv_test: dgemv_test.o
	${LD} -o $@ $^ -lblas
clean:
	rm -f *.o ${APPS}

